import asyncio
import json
import time
import datetime
import requests
import aiohttp
import websockets
from collections import defaultdict

# ================= CONFIGURATION =================
WS_URL = "wss://ws-subscriptions-clob.polymarket.com/ws/market"
GAMMA_API_URL = "https://gamma-api.polymarket.com/events"
GRAPH_URL = "https://api.goldsky.com/api/public/project_cl6mb8i9h0003e201j6li0diw/subgraphs/orderbook-subgraph/0.0.1/gn"


# ================= SETUP =================
def get_current_btc_slug():
    now = datetime.datetime.now(datetime.timezone.utc).timestamp()
    start_time_mark = (now // 900) * 900
    timestamp = int(start_time_mark)
    return f"btc-updown-15m-{timestamp}"


def fetch_market_details(slug):
    try:
        url = f"{GAMMA_API_URL}?slug={slug}"
        resp = requests.get(url)
        if resp.status_code == 404:
            ts_part = int(slug.split('-')[-1])
            url = f"{GAMMA_API_URL}?slug=btc-updown-15m-{ts_part + 900}"
            resp = requests.get(url)
        resp.raise_for_status()
        data = resp.json()
        event = data[0] if isinstance(data, list) else data
        market = event['markets'][0]
        return {
            "condition_id": market['conditionId'],
            "token_ids": json.loads(market['clobTokenIds']),
            "slug": event.get('slug')
        }
    except Exception as e:
        print(f"[!] Init Error: {e}")
        return None


# ================= VALIDATOR =================

class LiveValidator:
    def __init__(self, market_data):
        self.market_data = market_data
        self.token_ids = set(str(t) for t in market_data['token_ids'])

        # Buckets: { timestamp_sec: { price_float: TOTAL_VOLUME_FLOAT } }
        self.ws_volume = defaultdict(lambda: defaultdict(float))

        self.last_graph_timestamp = int(time.time()) - 300
        self.running = True

        # NEW: Track when we actually started listening
        self.stream_start_time = None

        print(f"[*] Watching Market: {market_data['slug']}")

    async def run_ws(self):
        print("[A] WS Connecting...")
        async with websockets.connect(WS_URL) as websocket:
            print("[A] WS Connected. Stream started.")

            # Subscribe
            await websocket.send(json.dumps({
                "type": "market",
                "assets_ids": list(self.token_ids)
            }))

            # Mark the start time (Add 1s buffer to be safe)
            self.stream_start_time = int(time.time()) + 1
            print(f"[*] Filter Active: Ignoring trades before TS {self.stream_start_time}")

            while self.running:
                try:
                    msg = await websocket.recv()
                    if not msg: continue
                    data = json.loads(msg)

                    items = data if isinstance(data, list) else [data]
                    for item in items:
                        if item.get('event_type') == "last_trade_price":
                            price = float(item['price'])
                            size = float(item['size'])
                            ts_raw = int(item.get('timestamp', 0))
                            ts_sec = int(ts_raw / 1000)

                            # 1. LOGGING
                            print(f"[WS] > Trade: {size:<8} @ {price:<5}")

                            # 2. VOLUME BUCKETING
                            # We sum the SIZE (Volume), not just count
                            self.ws_volume[ts_sec][price] += size

                except Exception as e:
                    print(f"[A] WS Error: {e}")
                    break

    async def run_graph(self):
        print("[B] Graph Poller Started.")

        query_template = """
        {
            ordersMatchedEvents(
                where: { timestamp_gte: "%s" }
                orderBy: timestamp
                orderDirection: asc
                first: 1000
            ) {
                timestamp
                makerAssetID
                takerAssetID
                makerAmountFilled
                takerAmountFilled
            }
        }
        """

        async with aiohttp.ClientSession() as session:
            while self.running:
                await asyncio.sleep(10)

                try:
                    query = query_template % self.last_graph_timestamp
                    async with session.post(GRAPH_URL, json={'query': query}) as resp:
                        if resp.status != 200: continue
                        body = await resp.json()
                        events = body.get('data', {}).get('ordersMatchedEvents', [])
                        if not events: continue

                        self.last_graph_timestamp = int(events[-1]['timestamp'])
                        self.process_graph_batch(events)

                except Exception as e:
                    print(f"[B] Poller Exception: {e}")

    def process_graph_batch(self, events):
        # Temp storage for Graph Activity
        graph_activity = defaultdict(list)

        for ev in events:
            # 1. FILTER: Ignore trades that happened before WS connected
            ts = int(ev['timestamp'])

            if self.stream_start_time is None or ts < self.stream_start_time:
                # Skip history
                continue

            # Filter for tokens
            if (str(ev['makerAssetID']) not in self.token_ids and
                    str(ev['takerAssetID']) not in self.token_ids):
                continue

            m = float(ev['makerAmountFilled'])
            t = float(ev['takerAmountFilled'])
            if m == 0 or t == 0: continue

            # Calculate Price (Both ways)
            p1 = round(t / m, 2)
            p2 = round(m / t, 2)

            graph_activity[ts].append([p1, p2])

        if not graph_activity:
            return

        print(f"\n--- VALIDATION SYNC ({len(graph_activity)} Active Seconds) ---")

        matches = 0
        misses = 0

        sorted_times = sorted(graph_activity.keys())

        for g_ts in sorted_times:
            prices_in_graph = graph_activity[g_ts]  # List of [p1, p2] pairs

            # Check WS Bucket for this Time Window (+/- 2s)
            ws_vol_found = 0.0
            is_matched = False

            # Look in WS Memory
            for t_chk in range(g_ts - 2, g_ts + 3):
                if t_chk in self.ws_volume:
                    # We have WS Volume in this second
                    ws_prices = self.ws_volume[t_chk]  # {price: volume}

                    # Does any WS price match the Graph Prices?
                    for w_p, w_vol in ws_prices.items():
                        for (gp1, gp2) in prices_in_graph:
                            # Fuzzy price match
                            if abs(w_p - gp1) < 0.02 or abs(w_p - gp2) < 0.02:
                                ws_vol_found += w_vol
                                is_matched = True

            if is_matched:
                matches += 1
                print(f"✅ TS {g_ts}: Graph Confirmed. WS Aggregated Vol: {ws_vol_found:.4f}")
            else:
                # Double check lag - only report miss if it's > 10s old
                if time.time() - g_ts > 10:
                    misses += 1
                    # Flatten list of prices for display
                    flat_prices = [p for pair in prices_in_graph for p in pair if p < 1.0 and p > 0.01]
                    print(f"❌ TS {g_ts}: MISSING IN WS! Graph had activity at prices {set(flat_prices)}")

        print(f"STATS: {matches} Windows Validated | {misses} Unmatched Gaps")
        print("---------------------------------------------------\n")

    async def run(self):
        await asyncio.gather(self.run_ws(), self.run_graph())


if __name__ == "__main__":
    slug = get_current_btc_slug()
    market = fetch_market_details(slug)
    if market:
        try:
            asyncio.run(LiveValidator(market).run())
        except KeyboardInterrupt:
            print("Stopped.")
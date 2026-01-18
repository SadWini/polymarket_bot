import requests
import json

# The Orderbook Subgraph URL
GRAPH_URL = "https://api.goldsky.com/api/public/project_cl6mb8i9h0003e201j6li0diw/subgraphs/orderbook-subgraph/0.0.1/gn"

# Standard Introspection Query
query = """
{
  __schema {
    queryType {
      fields {
        name
      }
    }
  }
}
"""

print(f"[*] Inspecting Schema at: {GRAPH_URL}...")
try:
    resp = requests.post(GRAPH_URL, json={'query': query})
    data = resp.json()

    if 'errors' in data:
        print("Error:", data['errors'])
    else:
        fields = [f['name'] for f in data['data']['__schema']['queryType']['fields']]
        print("\n✅ AVAILABLE TABLES (Entities):")
        print("-------------------------------")
        for f in fields:
            print(f" - {f}")
        print("-------------------------------")

except Exception as e:
    print(f"Failed: {e}")
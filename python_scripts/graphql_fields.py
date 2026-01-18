import requests
import json

GRAPH_URL = "https://api.goldsky.com/api/public/project_cl6mb8i9h0003e201j6li0diw/subgraphs/orderbook-subgraph/0.0.1/gn"

# We ask the API: "What fields exist on OrdersMatchedEvent?"
query = """
{
  __type(name: "OrdersMatchedEvent") {
    fields {
      name
      type {
        name
        kind
      }
    }
  }
}
"""

print(f"[*] Inspecting Schema...")
try:
    resp = requests.post(GRAPH_URL, json={'query': query})
    data = resp.json()

    if 'errors' in data:
        print("Error:", data['errors'])
    else:
        fields = data['data']['__type']['fields']
        print("\n✅ VALID FIELDS for 'OrdersMatchedEvent':")
        print("----------------------------------------")
        for f in fields:
            # Check if it's a simple value or a link to another table
            type_name = f['type']['name'] if f['type']['name'] else "Object/List"
            print(f" - {f['name']} ({type_name})")
        print("----------------------------------------")

except Exception as e:
    print(f"Failed: {e}")
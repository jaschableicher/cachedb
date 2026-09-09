| Command | Arguments / Parameters | Description |
|---|---|---|
| `SET` | `<key> <value>` | Sets a key to a specific value with an optional time-to-live. For strings with spaces, wrap them in quotations "<value>" |
| `GET` | `<key>` | Retrieves the value associated with the specified key. |
| `DEL` | `<key>` | Removes specified keys. |
| `EXPIRE` | `<key> <value>` | Expires key in value seconds. Returns -1 if key is non existent |
| `TTL` | `<key>` | Returns remaining seconds until expiration. Returns (nil) if non existent expiration or key |
| `SAVE` | `[location]` | Creates a Snapshot of memory. Standard to database.db file. Returns "ERR:..." if fails |
| `LOAD_SNAPSHOT` | `[location]` | Loads in memory snapshot. Standard from database.db file |

"""server.src.net — the modular GSS transport for the faithful host.

Extracted verbatim (byte-exact behavior) from the legacy Res/gss_stub_server_v4.py so the
transport lives under server/ as importable modules, not a monolith. Layers:
  crypto.py   — keystream load + XOR + ServerCryptoWriter (the wire cipher)
  framing.py  — lower-GSS packet parse + zlib frame (de)coders
  packets.py  — the builders (lower-GSS bodies, ClientWorld ops, RPC downcalls, component tails)
  login.py    — the clean E1/E2 login sequence (the one live lane)
  connection.py — the async per-client handler + crypto handshake
The composed entry point is server/run_server.py.
"""

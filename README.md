# Serve static files.

HTML files are only served without their extension (`/foo/bar.html` will only be served at `/foo/bar`).

This is a *single-threaded server*. That means a denial-of-service attack is trivial; `nc localhost 3000` does nicely.

This server is not *secure*. It is not *battle-tested*.

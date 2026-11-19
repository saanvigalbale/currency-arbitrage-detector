
import http.server
import json
import os
import subprocess
import tempfile
from pathlib import Path
from email.parser import BytesParser
from email.policy import default

ROOT = Path(__file__).resolve().parent
WEB = ROOT / "web"
EXE = ROOT / "build" / "currency_arbitrage.exe"


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB), **kwargs)

    def do_POST(self):
        if self.path != "/api/analyze":
            self.send_error(404)
            return

        csv_path = None

        try:
            content_type = self.headers.get("Content-Type", "")
            length = int(self.headers.get("Content-Length", "0"))

            if length <= 0 or length > 10 * 1024 * 1024:
                raise ValueError("Please upload a CSV file smaller than 10 MB.")

            body = self.rfile.read(length)
            message = BytesParser(policy=default).parsebytes(
                b"Content-Type: " + content_type.encode() +
                b"\r\nMIME-Version: 1.0\r\n\r\n" + body
            )

            uploaded_data = None
            capital = "1000"

            if not message.is_multipart():
                raise ValueError("Invalid upload format.")

            for part in message.iter_parts():
                name = part.get_param("name", header="content-disposition")

                if name == "file":
                    uploaded_data = part.get_payload(decode=True)
                elif name == "capital":
                    capital = part.get_content().strip()

            if not uploaded_data:
                raise ValueError("Please select a CSV file.")

            try:
                capital_value = float(capital)
                if not capital_value > 0 or not capital_value < float("inf"):
                    raise ValueError()
            except ValueError:
                raise ValueError("Starting capital must be a positive number.")

            with tempfile.NamedTemporaryFile(
                mode="wb", suffix=".csv", delete=False
            ) as temp:
                temp.write(uploaded_data)
                csv_path = temp.name

            result = subprocess.run(
                [str(EXE), csv_path, capital, "--json"],
                capture_output=True,
                text=True,
                timeout=15,
            )

            if not result.stdout.strip():
                raise RuntimeError(result.stderr or "Backend returned no output.")

            response = json.loads(result.stdout)
            self.send_json(response)

        except Exception as error:
            self.send_json({
                "status": "error",
                "analysis": None,
                "error": {"type": "server", "message": str(error)}
            })

        finally:
            if csv_path and os.path.exists(csv_path):
                os.unlink(csv_path)

    def send_json(self, data):
        body = json.dumps(data).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


if __name__ == "__main__":
    if not EXE.exists():
        raise SystemExit("C++ executable not found. Build the project first.")

    server = http.server.ThreadingHTTPServer(
        ("127.0.0.1", 8000), Handler
    )
    print("Currency Arbitrage Detector running at http://localhost:8000")
    print("Press Ctrl+C to stop.")
    server.serve_forever()

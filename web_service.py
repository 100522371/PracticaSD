from http.server import BaseHTTPRequestHandler, HTTPServer

class MessageService(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/normalize":
            self.send_response(404)
            self.end_headers()
            return

        # Tamaño del mensaje
        content_length = int(self.headers['Content-Length'])

        # Leer el mensaje
        data = self.rfile.read(content_length).decode()

        # Quitamos espacios repetidos (normalizar)
        normalized = " ".join(data.split())

        #Enviar respuesta
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.end_headers()

        self.wfile.write(normalized.encode())

def run():
        server = HTTPServer(('127.0.0.1', 8080), MessageService)
        print("Servicio web escuchando en http://127.0.0.1:8080")
        server.serve_forever()    

if __name__ == "__main__":
        run()
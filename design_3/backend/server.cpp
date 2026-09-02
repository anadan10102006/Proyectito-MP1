// ============================================================
//  DISEÑO 3 — UN HILO POR CADA TIPO (COLOR) DE VEHÍCULO
//
//  Partiendo del server.cpp original (Diseño 1: un hilo por carro).
//
//      Diseño 1:  6 carros  ->  6 hilos   (1 hilo = 1 carro)
//      Diseño 3:  6 carros  ->  3 hilos   (1 hilo = 1 color)
//
//  Lo único que cambia respecto al original:
//      - struct Car          : ahora tiene 'color'
//      - struct CarGroup     : NUEVO, agrupa los carros de un mismo color
//      - sendPosition()      : igual formato, ahora incluye el color
//      - updateCar()         : pasa a ser updateCarType(), recorre TODO el grupo
//      - main()              : crea 3 hilos (uno por color), no 6
//
//  Todo lo demás (base64, sha1, handshake, sendMessage) queda igual.
//
//  Compilar:  g++ -O2 -pthread -std=c++17 -o server server.cpp
//  Ejecutar:  ./server
// ============================================================
 
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include <chrono>
#include <string>
#include <sstream>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <csignal>
#include <unistd.h>
#include <arpa/inet.h>
 
using namespace std;
 
#define PORT 5000
 
 
// ============================================================
// CONFIGURACION DEL JUEGO
// ============================================================
 
// Los 6 carriles de la carretera (posiciones X).
const int LANES_X[] = {
    160,
    224,
    288,
    352,
    416,
    480
};
 
const int NUMBER_OF_LANES = 6;
 
// Distancia minima vertical entre enemigos del mismo carril.
const int MIN_VERTICAL_DISTANCE = 100;
 
// Tiempo entre actualizaciones.
const int UPDATE_TIME_MS = 30;
 
 
// ------------------------------------------------------------
//  DEFINICION DE LOS TIPOS DE VEHICULO
//
//  Aqui esta el corazon del Diseño 3: los carros se agrupan por
//  color y cada grupo tendra UN hilo.
//
//  3 colores x 2 carros = 6 carros en total, pero solo 3 hilos.
//
//  Cada color es dueño de 2 carriles. Al ser carriles distintos
//  por color, un hilo nunca necesita leer los carros de otro
//  hilo, y por eso cada grupo puede tener su propio mutex en
//  vez de compartir uno global.
//
//  AGREGAR UN COLOR NUEVO = AGREGAR UNA LINEA AQUI.
//  El servidor crea el hilo solo. (Pregunta 2 del enunciado.)
// ------------------------------------------------------------
 
struct CarTypeConfig {
    string      color;
    int         speed;
    vector<int> lanes;
    int         numberOfCars;
};
 
const vector<CarTypeConfig> CAR_TYPES = {
    {"red",   2, {160, 224}, 2},
    {"green", 3, {288, 352}, 2},
    {"blue",  4, {416, 480}, 2}
};
 
 
// ------------------------------------------------------------
//  MODO DE CARRILES  <-- el interruptor para el informe
//
//  true  : cada color tiene sus propios carriles. Los hilos son
//          independientes -> mutex por grupo -> PARALELISMO REAL.
//
//  false : los 3 colores comparten los 6 carriles. Para revisar
//          colisiones cada hilo debe leer los carros de los otros
//          colores -> hace falta un mutex GLOBAL -> los hilos se
//          serializan y el Diseño 3 se comporta como el Diseño 2
//          aunque existan 3 hilos.
//
//  Correr con los dos valores y comparar es la evidencia para la
//  pregunta 1 del enunciado.
// ------------------------------------------------------------
 
const bool PARTITIONED_LANES = true;
 
 
// ============================================================
// SOCKET
// ============================================================
 
// CAMBIO: antes era 'int clientSocket'. Lo escribe main y lo leen
// todos los hilos, asi que como int simple es una condicion de
// carrera (ThreadSanitizer la reporta). Con atomic queda correcto.
atomic<int> clientSocket{-1};
 
mutex socketMutex;
 
// Bandera de la CONEXION actual. Se baja cuando el cliente se
// desconecta (por ejemplo si recargas la pagina del navegador).
atomic<bool> running{true};
 
// Bandera de apagado TOTAL del programa (Ctrl+C).
// En el original los hilos hacian while(true) y el programa nunca
// terminaba; habia que matarlo a la fuerza.
atomic<bool> shuttingDown{false};
 
 
// ============================================================
// CARRO
// ============================================================
 
struct Car {
    int    id;
    int    x;
    int    y;
    int    speed;
    string color;   // CAMBIO: el carro ahora sabe de que tipo es
};
 
 
// ============================================================
// GRUPO DE CARROS  (NUEVO)
//
// Un grupo = todos los carros de un mismo color + el mutex que
// los protege. A cada grupo le corresponde exactamente un hilo.
// ============================================================
 
struct CarGroup {
    string            color;
    vector<Car>       cars;
    vector<int>       lanes;
    mutex             carsMutex;        // protege 'cars' de ESTE grupo
    atomic<long long> updates{0};       // metrica para el informe
};
 
// Mutex global: solo se usa cuando PARTITIONED_LANES es false.
mutex globalCarsMutex;
 
 
// ============================================================
// BASE64   (igual que el original)
// ============================================================
 
string base64Encode(const unsigned char* data, int length) {
 
    const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
 
    string result;
 
    for (int i = 0; i < length; i += 3) {
 
        int value = data[i] << 16;
 
        if (i + 1 < length)
            value |= data[i + 1] << 8;
 
        if (i + 2 < length)
            value |= data[i + 2];
 
        result += table[(value >> 18) & 63];
        result += table[(value >> 12) & 63];
 
        if (i + 1 < length)
            result += table[(value >> 6) & 63];
        else
            result += '=';
 
        if (i + 2 < length)
            result += table[value & 63];
        else
            result += '=';
    }
 
    return result;
}
 
 
// ============================================================
// SHA-1   (igual que el original)
// ============================================================
 
uint32_t rotateLeft(uint32_t value, int bits) {
 
    return (value << bits) | (value >> (32 - bits));
}
 
 
void sha1(const string& input, unsigned char output[20]) {
 
    string data = input;
 
    unsigned long long bitLength = data.size() * 8ULL;
 
    data += static_cast<char>(0x80);
 
    while ((data.size() % 64) != 56)
        data += static_cast<char>(0);
 
    for (int i = 7; i >= 0; i--)
        data += static_cast<char>((bitLength >> (i * 8)) & 0xFF);
 
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;
 
    for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
 
        uint32_t w[80];
 
        for (int i = 0; i < 16; i++) {
 
            w[i] =
                (static_cast<unsigned char>(data[chunk + i * 4])     << 24) |
                (static_cast<unsigned char>(data[chunk + i * 4 + 1]) << 16) |
                (static_cast<unsigned char>(data[chunk + i * 4 + 2]) << 8)  |
                 static_cast<unsigned char>(data[chunk + i * 4 + 3]);
        }
 
        for (int i = 16; i < 80; i++) {
 
            w[i] = rotateLeft(
                w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16],
                1
            );
        }
 
        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;
 
        for (int i = 0; i < 80; i++) {
 
            uint32_t f;
            uint32_t k;
 
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
 
            uint32_t temp =
                rotateLeft(a, 5) + f + e + k + w[i];
 
            e = d;
            d = c;
            c = rotateLeft(b, 30);
            b = a;
            a = temp;
        }
 
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }
 
    uint32_t hash[5] = { h0, h1, h2, h3, h4 };
 
    for (int i = 0; i < 5; i++) {
 
        output[i * 4]     = (hash[i] >> 24) & 0xFF;
        output[i * 4 + 1] = (hash[i] >> 16) & 0xFF;
        output[i * 4 + 2] = (hash[i] >> 8)  & 0xFF;
        output[i * 4 + 3] =  hash[i]        & 0xFF;
    }
}
 
 
// ============================================================
// HANDSHAKE WEBSOCKET   (igual que el original)
// ============================================================
 
bool websocketHandshake() {
 
    char buffer[4096];
 
    memset(buffer, 0, sizeof(buffer));
 
    int bytes = recv(
        clientSocket.load(),
        buffer,
        sizeof(buffer) - 1,
        0
    );
 
    if (bytes <= 0)
        return false;
 
    string request(buffer);
 
    string keyText = "Sec-WebSocket-Key:";
 
    size_t position = request.find(keyText);
 
    if (position == string::npos)
        return false;
 
    position += keyText.length();
 
    while (position < request.length() && request[position] == ' ')
        position++;
 
    size_t end = request.find("\r\n", position);
 
    if (end == string::npos)
        return false;
 
    string clientKey = request.substr(position, end - position);
 
    string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
 
    unsigned char hash[20];
 
    sha1(clientKey + magic, hash);
 
    string acceptKey = base64Encode(hash, 20);
 
    string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
 
    send(
        clientSocket.load(),
        response.c_str(),
        response.length(),
        0
    );
 
    return true;
}
 
 
// ============================================================
// ENVIAR MENSAJE WEBSOCKET
// ============================================================
 
static bool sendAll(int fd, const unsigned char* data, size_t n) {
 
    size_t sent = 0;
 
    while (sent < n) {
 
        ssize_t r = send(fd, data + sent, n - sent, 0);
 
        if (r <= 0)
            return false;
 
        sent += static_cast<size_t>(r);
    }
 
    return true;
}
 
 
bool sendMessage(const string& message) {
 
    lock_guard<mutex> lock(socketMutex);
 
    int fd = clientSocket.load();
 
    if (fd < 0)
        return false;
 
    vector<unsigned char> frame;
 
    frame.push_back(0x81);   // FIN + opcode texto
 
    size_t len = message.length();
 
    // El original hacia  header[1] = message.length()  lo cual solo
    // es valido hasta 125 bytes. Se agrega la longitud extendida por
    // seguridad, por si mas adelante se manda mas informacion.
    if (len <= 125) {
 
        frame.push_back(static_cast<unsigned char>(len));
 
    } else if (len <= 65535) {
 
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
 
    } else {
 
        frame.push_back(127);
 
        for (int i = 7; i >= 0; i--)
            frame.push_back((len >> (i * 8)) & 0xFF);
    }
 
    frame.insert(frame.end(), message.begin(), message.end());
 
    return sendAll(fd, frame.data(), frame.size());
}
 
 
// ============================================================
// ENVIAR POSICION
//
// IMPORTANTE: se conserva EXACTAMENTE el mismo formato que el
// original, un mensaje por carro:
//
//     {"id":1,"x":160,"y":-100}
//
// De esta manera game.js NO necesita ningun cambio.
//
// Se agrega "color" como campo extra. game.js actual solo lee
// id, x, y, asi que ignora el campo de mas sin problema, pero
// queda disponible por si despues quieren pintar cada tipo con
// su sprite correspondiente.
// ============================================================
 
void sendPosition(const Car& car) {
 
    stringstream message;
 
    message
        << "{"
        << "\"id\":"     << car.id
        << ","
        << "\"x\":"      << car.x
        << ","
        << "\"y\":"      << car.y
        << ","
        << "\"color\":\"" << car.color << "\""
        << "}";
 
    sendMessage(message.str());
}
 
 
// ============================================================
// COMPROBAR COLISION ENTRE ENEMIGOS
//
// 'universe' son los conjuntos de carros contra los que hay que
// comparar:
//     - modo particionado : solo el propio grupo
//     - modo compartido   : todos los grupos
// ============================================================
 
bool positionIsSafe(
    int carId,
    int carX,
    int proposedY,
    const vector<const vector<Car>*>& universe
) {
 
    for (const vector<Car>* group : universe) {
 
        for (const Car& otherCar : *group) {
 
            // No compararnos con nosotros mismos
            if (otherCar.id == carId)
                continue;
 
            // Solo existe riesgo de choque si estan
            // en el mismo carril.
            if (otherCar.x != carX)
                continue;
 
            int distance = proposedY - otherCar.y;
 
            if (distance < 0)
                distance = -distance;
 
            // Si estan demasiado cerca,
            // no permitimos la nueva posicion.
            if (distance < MIN_VERTICAL_DISTANCE)
                return false;
        }
    }
 
    return true;
}
 
 
// ============================================================
// THREAD DE CADA TIPO (COLOR)   <-- antes era updateCar()
//
// En el Diseño 1 esta funcion manejaba UN carro.
// En el Diseño 3 maneja TODOS los carros de un color.
// ============================================================
 
void updateCarType(
    CarGroup* group,
    vector<unique_ptr<CarGroup>>* allGroups
) {
 
    cout
        << "[HILO " << group->color << "] iniciado, maneja "
        << group->cars.size() << " carros"
        << endl;
 
    // Generador propio de cada hilo.
    // OJO: rand() NO es seguro entre hilos porque comparte estado
    // interno. Por eso cada hilo usa su propio mt19937.
    mt19937 rng(
        static_cast<unsigned>(
            chrono::steady_clock::now().time_since_epoch().count()
        ) + static_cast<unsigned>(group->color[0])
    );
 
    long long iteration = 0;
 
    while (running.load()) {
 
        // Copia de las posiciones para enviarlas DESPUES de soltar
        // el mutex. En el original sendPosition() se llamaba dentro
        // de la seccion critica, o sea que una escritura de red
        // bloqueante ocurria con el candado tomado.
        vector<Car> snapshot;
 
        int moved   = 0;
        int blocked = 0;
 
        {
            // ----------------------------------------------------
            // SECCION CRITICA
            //
            // La granularidad del candado depende del modo. Esta es
            // LA decision de diseño que define si hay paralelismo
            // real o si los hilos se serializan.
            // ----------------------------------------------------
            unique_lock<mutex> lock = PARTITIONED_LANES
                ? unique_lock<mutex>(group->carsMutex)
                : unique_lock<mutex>(globalCarsMutex);
 
            vector<const vector<Car>*> universe;
 
            if (PARTITIONED_LANES) {
                universe.push_back(&group->cars);
            } else {
                for (auto& g : *allGroups)
                    universe.push_back(&g->cars);
            }
 
            // ----------------------------------------------------
            // ACTUALIZAR TODOS LOS CARROS DE ESTE COLOR
            // ----------------------------------------------------
            for (Car& car : group->cars) {
 
                int proposedY = car.y + car.speed;
 
                if (positionIsSafe(car.id, car.x, proposedY, universe)) {
                    car.y = proposedY;
                    moved++;
                } else {
                    blocked++;
                }
 
                // --------------------------------------------
                // SI SALE DE LA PANTALLA LO DEVOLVEMOS ARRIBA
                //
                // Reaparece en un carril libre de los que le
                // pertenecen a su color, elegido al azar.
                // --------------------------------------------
                if (car.y > 900) {
 
                    vector<int> candidates = group->lanes;
                    shuffle(candidates.begin(), candidates.end(), rng);
 
                    for (int lane : candidates) {
 
                        if (positionIsSafe(car.id, lane, -100, universe)) {
                            car.x = lane;
                            car.y = -100;
                            break;
                        }
                    }
                    // Si ningun carril esta libre se queda abajo y
                    // vuelve a intentar en la siguiente iteracion.
                }
            }
 
            snapshot = group->cars;
 
        } // <-- aqui se libera el mutex
 
        // ----------------------------------------------------
        // ENVIAR POSICIONES AL FRONTEND (ya sin el candado)
        // Un mensaje por carro, igual que en el original.
        // ----------------------------------------------------
        for (const Car& car : snapshot) {
 
            if (!sendMessage(
                    "{\"id\":" + to_string(car.id) +
                    ",\"x\":"  + to_string(car.x)  +
                    ",\"y\":"  + to_string(car.y)  +
                    ",\"color\":\"" + car.color + "\"}"
                )) {
 
                cout
                    << "[HILO " << group->color << "] cliente desconectado"
                    << endl;
 
                running = false;
                return;
            }
        }
 
        group->updates++;
        iteration++;
 
        // Imprimir mas o menos una vez por segundo para no
        // inundar la terminal (6 carros x 33 veces por segundo).
        if (iteration % 33 == 0) {
 
            cout << "[" << group->color << "] it=" << iteration;
 
            for (const Car& car : snapshot)
                cout << " | C" << car.id
                     << " x=" << car.x
                     << " y=" << car.y;
 
            cout << " | movidos=" << moved
                 << " bloqueados=" << blocked
                 << endl;
        }
 
        this_thread::sleep_for(
            chrono::milliseconds(UPDATE_TIME_MS)
        );
    }
 
    cout
        << "[HILO " << group->color << "] terminado tras "
        << group->updates.load() << " actualizaciones"
        << endl;
}
 
 
// ============================================================
// SEÑAL PARA APAGAR LIMPIO (Ctrl+C)
// ============================================================
 
void handleSignal(int) {
    shuttingDown = true;
    running      = false;
}
 
 
// ============================================================
// MAIN
// ============================================================
 
int main() {
 
    signal(SIGPIPE, SIG_IGN);
 
    // Se usa sigaction en vez de signal() para que Ctrl+C interrumpa el
    // accept() en vez de reiniciarlo (signal() activa SA_RESTART y el
    // programa se quedaria pegado esperando conexion).
    struct sigaction sa{};
    sa.sa_handler = handleSignal;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
 
 
    // --------------------------------------------------------
    // CREAR LOS GRUPOS DE CARROS (uno por color)
    //
    // En el original era un vector<Car> plano. Ahora los carros
    // quedan agrupados por color, porque cada grupo va a tener
    // su propio hilo.
    // --------------------------------------------------------
 
    vector<unique_ptr<CarGroup>> groups;
 
    int nextId      = 1;
    int totalCars   = 0;
    int globalIndex = 0;   // para repartir carriles en modo compartido
 
    for (const CarTypeConfig& config : CAR_TYPES) {
 
        auto group   = make_unique<CarGroup>();
        group->color = config.color;
 
        if (PARTITIONED_LANES) {
            group->lanes = config.lanes;
        } else {
            group->lanes.assign(
                LANES_X,
                LANES_X + NUMBER_OF_LANES
            );
        }
 
        const int laneCount = static_cast<int>(group->lanes.size());
 
        for (int i = 0; i < config.numberOfCars; i++) {
 
            // En modo compartido la colocacion tiene que ser GLOBAL:
            // si cada color empezara desde cero, los primeros carros
            // de cada color nacerian en el mismo carril y la misma Y
            // y quedarian bloqueados desde el arranque.
            const int index = PARTITIONED_LANES ? i : globalIndex;
 
            Car car;
 
            car.id    = nextId++;
            car.x     = group->lanes[index % laneCount];
            car.y     = -100 - (index / laneCount) * 220;
            car.speed = config.speed;
            car.color = config.color;
 
            group->cars.push_back(car);
 
            totalCars++;
            globalIndex++;
        }
 
        groups.push_back(move(group));
    }
 
 
    // --------------------------------------------------------
    // CREAR SOCKET
    // --------------------------------------------------------
 
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
 
    if (serverSocket < 0) {
        cerr << "Error creando socket." << endl;
        return 1;
    }
 
    int option = 1;
 
    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &option,
        sizeof(option)
    );
 
 
    sockaddr_in serverAddress{};
 
    serverAddress.sin_family      = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port        = htons(PORT);
 
 
    if (bind(serverSocket,
             (sockaddr*)&serverAddress,
             sizeof(serverAddress)) < 0) {
 
        cerr << "Error en bind." << endl;
        close(serverSocket);
        return 1;
    }
 
 
    if (listen(serverSocket, 1) < 0) {
 
        cerr << "Error en listen." << endl;
        close(serverSocket);
        return 1;
    }
 
 
    cout << "====================================" << endl;
    cout << " SERVIDOR ENEMIGOS"                   << endl;
    cout << " PORT      : " << PORT                << endl;
    cout << " DISENO 3  : un hilo por TIPO/COLOR"  << endl;
    cout << " COLORES   : " << CAR_TYPES.size()
         << "  (= numero de hilos)"                << endl;
    cout << " CARROS    : " << totalCars           << endl;
    cout << " CARRILES  : "
         << (PARTITIONED_LANES ? "particionados por color"
                               : "compartidos (mutex global)") << endl;
    cout << "====================================" << endl;
 
    for (auto& g : groups) {
 
        cout << "  " << g->color << ": "
             << g->cars.size() << " carros, carriles {";
 
        for (size_t i = 0; i < g->lanes.size(); i++)
            cout << g->lanes[i]
                 << (i + 1 < g->lanes.size() ? "," : "");
 
        cout << "}" << endl;
    }
 
    // --------------------------------------------------------
    // BUCLE DE CONEXIONES
    //
    // El original aceptaba UNA sola conexion: si recargabas la
    // pagina del navegador, el servidor se cerraba y tocaba
    // volver a lanzarlo a mano. Ahora, cuando el cliente se va,
    // vuelve a quedar esperando una conexion nueva.
    //
    // Se sale de verdad solo con Ctrl+C.
    // --------------------------------------------------------
 
    while (!shuttingDown.load()) {
 
        cout << "\nEsperando conexion WebSocket..." << endl;
 
        sockaddr_in clientAddress{};
 
        socklen_t clientLength = sizeof(clientAddress);
 
        int cs = accept(
            serverSocket,
            (sockaddr*)&clientAddress,
            &clientLength
        );
 
        if (cs < 0) {
 
            // Ctrl+C interrumpe el accept: salimos del bucle.
            if (shuttingDown.load())
                break;
 
            cerr << "Error aceptando cliente." << endl;
            continue;
        }
 
        clientSocket = cs;
 
 
        // ----------------------------------------------------
        // HANDSHAKE
        // ----------------------------------------------------
 
        if (!websocketHandshake()) {
 
            cerr << "Error en el handshake WebSocket." << endl;
            close(cs);
            clientSocket = -1;
            continue;
        }
 
        cout << "WebSocket conectado.\n" << endl;
 
 
        // ----------------------------------------------------
        // CREAR LOS THREADS
        //
        // AQUI ESTA LA DIFERENCIA CENTRAL CON EL DISEÑO 1:
        //   Diseño 1 -> un hilo por cada carro   (6 hilos)
        //   Diseño 3 -> un hilo por cada color   (3 hilos)
        // ----------------------------------------------------
 
        running = true;
 
        for (auto& g : groups)
            g->updates = 0;
 
        auto startTime = chrono::steady_clock::now();
 
        vector<thread> threads;
 
        for (auto& group : groups)
            threads.emplace_back(updateCarType, group.get(), &groups);
 
 
        for (thread& t : threads)
            t.join();
 
 
        // ----------------------------------------------------
        // RESUMEN DE ESTA SESION (util para el informe)
        // ----------------------------------------------------
 
        auto endTime = chrono::steady_clock::now();
 
        double seconds =
            chrono::duration<double>(endTime - startTime).count();
 
        cout << "\n------------- RESUMEN -------------" << endl;
        cout << "Tiempo de ejecucion : " << seconds << " s" << endl;
 
        long long total = 0;
 
        for (auto& g : groups) {
 
            long long u = g->updates.load();
            total += u;
 
            cout << "  " << g->color << ": " << u
                 << " ciclos (" << (seconds > 0 ? u / seconds : 0)
                 << " /s)" << endl;
        }
 
        cout << "Total: " << total << " ciclos de grupo" << endl;
        cout << "-----------------------------------" << endl;
 
 
        close(clientSocket.load());
        clientSocket = -1;
    }
 
    cout << "\nCerrando servidor." << endl;
 
    close(serverSocket);
 
    return 0;
}
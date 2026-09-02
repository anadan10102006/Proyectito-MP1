#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <string>
#include <sstream>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <csignal>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

#define PORT 5000

// CONFIGURACION DEL JUEGO

const int NUMBER_OF_CARS = 6; // Ahora coincide con NUMBER_OF_LANES, así se usan los 6 carriles

// Posiciones de los 6 carriles de la carretera.
// Se dejan separados para evitar que los carros se monten.
const int LANES_X[] = {
    160,
    224,
    288,
    352,
    416,
    480};

const int NUMBER_OF_LANES = 6;

// Distancia minima vertical entre enemigos si estan
// en el mismo carril.
const int MIN_VERTICAL_DISTANCE = 100;

// Velocidad de cada carro
const int CAR_SPEED[] = {
    2,
    2,
    3,
    2,
    3,
    2};

// Tiempo entre actualizaciones
const int UPDATE_TIME_MS = 30;

// IMPLEMENTACIÓN PARA EL WEBSOCKET : CONECTARSE CON EL FRONT END.

int clientSocket = -1;

mutex socketMutex;

// CARRO

struct Car
{
    int id;
    int x;
    int y;
    int speed;
};

// BASE64

string base64Encode(const unsigned char *data, int length)
{

    const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    string result;

    for (int i = 0; i < length; i += 3)
    {

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

// SHA-1

uint32_t rotateLeft(uint32_t value, int bits)
{

    return (value << bits) | (value >> (32 - bits));
}

void sha1(const string &input, unsigned char output[20])
{

    string data = input;

    unsigned long long bitLength =
        data.size() * 8;

    data += static_cast<char>(0x80);

    while ((data.size() % 64) != 56)
        data += static_cast<char>(0);

    for (int i = 7; i >= 0; i--)
    {

        data += static_cast<char>(
            (bitLength >> (i * 8)) & 0xFF);
    }

    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    for (
        size_t chunk = 0;
        chunk < data.size();
        chunk += 64)
    {

        uint32_t w[80];

        for (int i = 0; i < 16; i++)
        {

            w[i] =
                (static_cast<unsigned char>(
                     data[chunk + i * 4])
                 << 24) |
                (static_cast<unsigned char>(
                     data[chunk + i * 4 + 1])
                 << 16) |
                (static_cast<unsigned char>(
                     data[chunk + i * 4 + 2])
                 << 8) |
                static_cast<unsigned char>(
                    data[chunk + i * 4 + 3]);
        }

        for (int i = 16; i < 80; i++)
        {

            w[i] = rotateLeft(
                w[i - 3] ^
                    w[i - 8] ^
                    w[i - 14] ^
                    w[i - 16],
                1);
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        for (int i = 0; i < 80; i++)
        {

            uint32_t f;
            uint32_t k;

            if (i < 20)
            {

                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            }
            else if (i < 40)
            {

                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (i < 60)
            {

                f =
                    (b & c) |
                    (b & d) |
                    (c & d);

                k = 0x8F1BBCDC;
            }
            else
            {

                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp =
                rotateLeft(a, 5) +
                f +
                e +
                k +
                w[i];

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

    uint32_t hash[5] = {
        h0, h1, h2, h3, h4};

    for (int i = 0; i < 5; i++)
    {

        output[i * 4] =
            (hash[i] >> 24) & 0xFF;

        output[i * 4 + 1] =
            (hash[i] >> 16) & 0xFF;

        output[i * 4 + 2] =
            (hash[i] >> 8) & 0xFF;

        output[i * 4 + 3] =
            hash[i] & 0xFF;
    }
}

// HANDSHAKE WEBSOCKET

bool websocketHandshake()
{

    char buffer[4096];

    memset(buffer, 0, sizeof(buffer));

    int bytes =
        recv(
            clientSocket,
            buffer,
            sizeof(buffer) - 1,
            0);

    if (bytes <= 0)
        return false;

    string request(buffer);

    string keyText =
        "Sec-WebSocket-Key:";

    size_t position =
        request.find(keyText);

    if (position == string::npos)
        return false;

    position += keyText.length();

    while (
        position < request.length() &&
        request[position] == ' ')
    {
        position++;
    }

    size_t end =
        request.find("\r\n", position);

    if (end == string::npos)
        return false;

    string clientKey =
        request.substr(
            position,
            end - position);

    string magic =
        "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    unsigned char hash[20];

    sha1(
        clientKey + magic,
        hash);

    string acceptKey =
        base64Encode(hash, 20);

    string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " +
        acceptKey +
        "\r\n\r\n";

    send(
        clientSocket,
        response.c_str(),
        response.length(),
        0);

    return true;
}

// ENVIAR MENSAJE WEBSOCKET

bool sendMessage(const string &message)
{

    lock_guard<mutex> lock(socketMutex);

    if (clientSocket < 0)
        return false;

    unsigned char header[2];

    header[0] = 0x81;

    header[1] =
        static_cast<unsigned char>(
            message.length());

    int result =
        send(
            clientSocket,
            header,
            2,
            0);

    if (result <= 0)
        return false;

    result =
        send(
            clientSocket,
            message.c_str(),
            message.length(),
            0);

    if (result <= 0)
        return false;

    return true;
}

// ENVIAR POSICION

bool sendPosition(const Car &car)
{

    stringstream message;

    message
        << "{"
        << "\"id\":" << car.id
        << ","
        << "\"x\":" << car.x
        << ","
        << "\"y\":" << car.y
        << "}";

    return sendMessage(
        message.str());
}

// ****************************************
// DISEÑO 4 - THREAD POOL + COLA DE TAREAS
// ****************************************

// Cantidad fija de hilos trabajadores, no depende de la cantidad de carros. Se escogieron 4 hilos trabajadores arbitrariamente.
const unsigned int NUMBER_OF_WORKERS = 4;

// La cola es compartida entre el hilo principal (productor de tareas) y los workers (consumidores).
queue<function<void()>> taskQueue;
mutex queueMutex;
condition_variable queueCondition;

// Permite detener el sistema cuando el cliente se desconecta o cuando termina el servidor.
atomic<bool> running(true);

// Protege el estado compartido de los carros.
mutex carsMutex;
mutex coutMutex;
thread_local int currentWorkerId = 0;

// ****************************************
// ACTUALIZAR UN CARRO - UNA SOLA TAREA
// ****************************************

bool updateCarOnce(int carIndex, vector<Car> &cars)
{

    Car snapshot;

    {
        // Se protege el acceso al estado compartido.
        // El cálculo y el envío se hacen fuera del mutex.
        lock_guard<mutex> lock(carsMutex);

        Car &car = cars[carIndex];

        car.y += car.speed;

        if (car.y > 900)
        {
            car.y = -100;
        }

        snapshot = car;
    }

    // Permite observar que distintos workers toman tareas de distintos carros.
    {
        lock_guard<mutex> lock(coutMutex);
        cout << "[Worker " << currentWorkerId
             << "] carro " << snapshot.id
             << " -> y=" << snapshot.y << endl;
    }

    // El socket también es un recurso compartido.
    if (!sendPosition(snapshot))
    {
        running = false;
        queueCondition.notify_all();
        return false;
    }

    return true;
}

// ****************************************
// WORKER
// ****************************************

void worker(int workerId)
{

    currentWorkerId = workerId;

    while (running.load())
    {

        function<void()> task;

        {
            unique_lock<mutex> lock(queueMutex);

            // El worker duerme hasta que exista trabajo
            // o se solicite terminar.
            queueCondition.wait(
                lock,
                []
                {
                    return !taskQueue.empty() || !running.load();
                });

            if (!running.load() && taskQueue.empty())
            {
                return;
            }

            if (taskQueue.empty())
            {
                continue;
            }

            // Sacamos UNA tarea de la cola.
            task = move(taskQueue.front());
            taskQueue.pop();
        }

        // Importante:
        // ejecutamos la tarea SIN tener bloqueado queueMutex.
        task();
    }
}

// ****************************************
// AGREGAR TAREA A LA COLA
// ****************************************

void enqueueTask(function<void()> task)
{

    {
        lock_guard<mutex> lock(queueMutex);
        taskQueue.push(move(task));
    }

    // Despertar a un worker que estuviera dormido.
    queueCondition.notify_one();
}

// ****************************************
// MAIN - DISEÑO 4
// ****************************************

int main()
{

    signal(SIGPIPE, SIG_IGN);

    // ****************************************
    // CREAR CARROS
    // ****************************************

    vector<Car> cars(NUMBER_OF_CARS);

    for (int i = 0; i < NUMBER_OF_CARS; i++)
    {

        cars[i].id = i + 1;
        cars[i].x = LANES_X[i % NUMBER_OF_LANES];
        cars[i].y = -100 - (i * 220);
        cars[i].speed = CAR_SPEED[i % 6];
    }

    // ****************************************
    // CREAR SOCKET
    // ****************************************

    int serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0);

    if (serverSocket < 0)
    {
        cerr << "Error creando socket." << endl;
        return 1;
    }

    int option = 1;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &option,
        sizeof(option));

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);

    // ****************************************
    // BIND
    // ****************************************

    if (
        bind(
            serverSocket,
            (sockaddr *)&serverAddress,
            sizeof(serverAddress)) < 0)
    {
        cerr << "Error en bind." << endl;
        close(serverSocket);
        return 1;
    }

    // ****************************************
    // LISTEN
    // ****************************************

    if (listen(serverSocket, 1) < 0)
    {
        cerr << "Error en listen." << endl;
        close(serverSocket);
        return 1;
    }

    cout << "====================================" << endl;
    cout << " SERVIDOR ENEMIGOS - DISENO 4" << endl;
    cout << " PORT: " << PORT << endl;
    cout << " WORKERS: " << NUMBER_OF_WORKERS << endl;
    cout << "====================================" << endl;
    cout << "Esperando conexion WebSocket..." << endl;

    // ****************************************
    // ACEPTAR FRONTEND
    // ****************************************

    sockaddr_in clientAddress{};
    socklen_t clientLength = sizeof(clientAddress);

    clientSocket =
        accept(
            serverSocket,
            (sockaddr *)&clientAddress,
            &clientLength);

    if (clientSocket < 0)
    {
        cerr << "Error aceptando cliente." << endl;
        close(serverSocket);
        return 1;
    }
    // ****************************************
    // HANDSHAKE
    // ****************************************

    if (!websocketHandshake())
    {
        cerr << "Error en WebSocket." << endl;
        close(clientSocket);
        close(serverSocket);
        return 1;
    }

    cout << "WebSocket conectado." << endl;

    // ****************************************
    // CREAR POOL FIJO DE WORKERS
    // ****************************************

    vector<thread> workers;

    for (unsigned int i = 0; i < NUMBER_OF_WORKERS; i++)
    {
        workers.emplace_back(worker, i + 1);
    }

    // ****************************************
    // PRODUCTOR DE TAREAS
    // ****************************************

    while (running.load())
    {

        auto nextUpdate =
            chrono::steady_clock::now() +
            chrono::milliseconds(UPDATE_TIME_MS);

        // Una tarea por carro.
        // Ningun carro posee un thread permanente.
        for (int i = 0; i < NUMBER_OF_CARS; i++)
        {

            enqueueTask(
                [i, &cars]()
                {
                    updateCarOnce(i, cars);
                });
        }

        // Mantener aproximadamente 30 ms entre lotes.
        this_thread::sleep_until(nextUpdate);
    }

    // ****************************************
    // DETENER WORKERS
    // ****************************************

    running = false;
    queueCondition.notify_all();

    for (thread &t : workers)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    close(clientSocket);
    close(serverSocket);

    return 0;
}

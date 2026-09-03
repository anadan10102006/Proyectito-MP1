# Enemy Cars - Multi-Threading and Shared-Memory Ideas

## Integrantes: Maria Lucía Castillo García, Juliana González Sánchez y Ana Daniela Paredes Tovar.

## Micro-Proyecto No. 1 - Programación Paralela

El objetivo de este proyecto es implementar y comparar diferentes estrategias de programación concurrente para la actualización del movimiento de vehículos enemigos en una simulación en tiempo real.

El proyecto utiliza una arquitectura **cliente-servidor**, en la cual:

- El **backend**, desarrollado en C++, genera y actualiza las posiciones de los vehículos enemigos.
- El **frontend**, desarrollado en JavaScript y PixiJS, representa el juego y actualiza visualmente las posiciones recibidas.
- La comunicación entre backend y frontend se realiza mediante **WebSockets**.
- Cada diseño utiliza una estrategia diferente para distribuir el trabajo entre los hilos.

El propósito principal es analizar las diferencias entre las estrategias en términos de:

- Paralelismo.
- Rendimiento.
- Uso de múltiples núcleos.
- Sincronización.
- Escalabilidad.
- Complejidad de implementación.
- Facilidad de mantenimiento.

---

# Objetivos

El proyecto busca aplicar de manera práctica los conceptos de programación paralela mediante:

- Creación y administración de hilos.
- Descomposición de un problema en tareas independientes.
- Actualización concurrente de información.
- Manejo de datos compartidos.
- Sincronización entre hilos.
- Comunicación entre procesos mediante WebSockets.
- Comparación de diferentes estrategias de concurrencia.

El enunciado plantea cuatro estrategias diferentes para la actualización de los vehículos enemigos:

1. Un hilo independiente por vehículo.
2. Un único hilo para todos los vehículos.
3. Un hilo por tipo de vehículo.
4. Un grupo fijo de hilos que ejecutan tareas tomadas de una cola.

---

# Arquitectura general

La aplicación está dividida en dos componentes principales:

```text
                    ┌─────────────────────────┐
                    │        FRONTEND         │
                    │                         │
                    │     JavaScript          │
                    │        + PixiJS         │
                    │                         │
                    │  Renderiza los vehículos │
                    └────────────┬────────────┘
                                 │
                              WebSocket
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │         BACKEND         │
                    │                         │
                    │           C++           │
                    │                         │
                    │ Actualiza posiciones de │
                    │ los vehículos mediante  │
                    │ diferentes estrategias  │
                    │ de concurrencia         │
                    └─────────────────────────┘
```

El backend genera periódicamente las nuevas posiciones de los vehículos y las envía al frontend mediante mensajes JSON.

Ejemplo de mensaje:

```json
{
  "id": 1,
  "x": 160,
  "y": 250
}
```

El frontend recibe estos datos y utiliza la posición recibida para actualizar la representación gráfica del vehículo correspondiente.

---

# Tecnologías utilizadas

## Backend

- C++
- C++ Threads (`std::thread`)
- `std::mutex`
- `std::atomic`
- `std::condition_variable`
- `std::queue`
- Programación con sockets
- WebSockets

## Frontend

- HTML5
- JavaScript
- PixiJS
- WebSocket API

## Infraestructura

- Docker
- Docker Compose

---

# Estructura del repositorio

Cada diseño se encuentra implementado en una **rama independiente**.

La estructura esperada del repositorio es:

```text
.
├── README.md
│
├── design_1/
│   ├── backend/
│   │   ├── Dockerfile
│   │   └── server.cpp
│   │
│   ├── frontend/
│   │   ├── Dockerfile
│   │   ├── index.html
│   │   ├── scripts/
│   │   ├── styles/
│   │   └── assets/
│   │
│   └── docker-compose.yml
│
├── design_2/
│   ├── backend/
│   ├── frontend/
│   └── docker-compose.yml
│
├── design_3/
│   ├── backend/
│   ├── frontend/
│   └── docker-compose.yml
│
└── design_4/
    ├── backend/
    ├── frontend/
    └── docker-compose.yml
```

Sin embargo, **cada diseño está almacenado en una rama diferente**.

De la siguiente forma:

```text
Rama design_1
└── design_1/
    ├── backend/
    ├── frontend/
    └── docker-compose.yml
```

```text
Rama design_2
└── design_2/
    ├── backend/
    ├── frontend/
    └── docker-compose.yml
```

```text
Rama design_3
└── design_3/
    ├── backend/
    ├── frontend/
    └── docker-compose.yml
```

```text
Rama design_4
└── design_4/
    ├── backend/
    ├── frontend/
    └── docker-compose.yml
```

De esta manera, cada rama contiene únicamente la implementación correspondiente al diseño que representa.

---

# Diseño 1 - Hilos independientes

## Descripción

En el **Diseño 1**, cada vehículo enemigo tiene su propio hilo de ejecución.

La distribución del trabajo es:

```text
Carro 1 ─────► Thread 1
Carro 2 ─────► Thread 2
Carro 3 ─────► Thread 3
Carro 4 ─────► Thread 4
Carro 5 ─────► Thread 5
Carro 6 ─────► Thread 6
```

Cada hilo es responsable de actualizar únicamente el vehículo que tiene asignado. La implementación utiliza `std::thread`. El estado de los vehículos se almacena en una estructura basada en `vector<Car>`.

Debido a que varios hilos pueden acceder al estado compartido de los vehículos, se utilizan mecanismos de sincronización para evitar accesos simultáneos incompatibles. También se controla el acceso al socket utilizado para enviar información al frontend.

## Ejecución

Cambiar a la rama correspondiente:

```bash
git checkout design_1
```

Entrar en la carpeta:

```bash
cd design_1
```

Construir y ejecutar:

```bash
docker compose up --build
```

Abrir el navegador:

```text
http://localhost:8080
```

Para detener la aplicación:

```bash
docker compose down
```

---

# Diseño 2 - Hilo único de actualización

## Descripción

En el **Diseño 2**, todos los vehículos son actualizados por un único hilo.

La distribución del trabajo es:

```text
                  Thread 1
                     │
          ┌──────────┼──────────┐
          ▼          ▼          ▼
        Carro 1    Carro 2    Carro N
```

Existe un único hilo encargado de recorrer la colección de vehículos y actualizar sus posiciones. A diferencia del Diseño 1, los vehículos no se actualizan mediante hilos independientes, por lo que la actualización de vehículos es esencialmente secuencial.


## Ejecución

Cambiar a la rama:

```bash
git checkout design_2
```

Entrar en la carpeta:

```bash
cd design_2
```

Construir y ejecutar:

```bash
docker compose up --build
```

Abrir:

```text
http://localhost:8080
```

Para detener:

```bash
docker compose down
```

---

# Diseño 3 - Un hilo por tipo de vehículo

## Descripción

En el **Diseño 3**, los vehículos se agrupan según su tipo o color.

Cada grupo de vehículos es administrado por un hilo independiente.

La distribución conceptual es:

```text
                  ┌───────────────────┐
                  │    Thread Red     │
                  │                   │
                  │ Car 1             │
                  │ Car 2             │
                  └───────────────────┘


                  ┌───────────────────┐
                  │   Thread Green    │
                  │                   │
                  │ Car 3             │
                  │ Car 4             │
                  └───────────────────┘


                  ┌───────────────────┐
                  │    Thread Blue    │
                  │                   │
                  │ Car 5             │
                  │ Car 6             │
                  └───────────────────┘
```

En esta implementación se utilizan grupos de vehículos representados mediante una estructura `CarGroup`.

Cada grupo puede contener información relacionada con:

- Tipo o color.
- Vehículos pertenecientes al grupo.
- Velocidad.
- Carriles utilizados.
- Sincronización correspondiente al grupo.

Esta estrategia busca obtener un punto intermedio entre los diseños 1 y 2. En lugar de crear un hilo por vehículo, se reduce la cantidad de hilos agrupando varios vehículos que comparten características. Existe un mayor paralelismo que el Diseño 2.

## Ejecución

Cambiar a la rama:

```bash
git checkout design_3
```

Entrar en:

```bash
cd design_3
```

Construir y ejecutar:

```bash
docker compose up --build
```

Abrir:

```text
http://localhost:8080
```

Para detener:

```bash
docker compose down
```

---

# Diseño 4 - Pool fijo de trabajadores y cola de tareas

## Descripción

En el **Diseño 4**, los vehículos no tienen un hilo permanente.

En su lugar, se crea un grupo fijo de hilos trabajadores que reciben tareas desde una cola.

La arquitectura es:

```text
                 ┌──────────────────────┐
                 │     Cola de tareas   │
                 └──────────┬───────────┘
                            │
             ┌──────────────┼──────────────┐
             ▼              ▼              ▼
         Worker 1       Worker 2       Worker 3
             │              │              │
             ▼              ▼              ▼
          Tarea          Tarea          Tarea

                         ...
                         
                    Worker N
```

El modelo utilizado corresponde a un esquema productor-consumidor. El sistema genera tareas de actualización para los vehículos y las coloca en una cola. Los hilos trabajadores toman las tareas disponibles y ejecutan las actualizaciones.

La cola utiliza:

```cpp
std::queue<std::function<void()>>
```

Para coordinar el acceso a la cola se utilizan:

```cpp
std::mutex
std::condition_variable
```

De esta manera, los trabajadores pueden esperar cuando no existen tareas y comenzar a trabajar cuando se agregan nuevas tareas. Permite controlar el número de hilos independientemente de la cantidad de vehículos. Por lo que hay una mayor flexibilidad para aumentar el número de vehículos.

## Ejecución

Cambiar a la rama:

```bash
git checkout design_4
```

Entrar en:

```bash
cd design_4
```

Construir y ejecutar:

```bash
docker compose up --build
```

Abrir:

```text
http://localhost:8080
```

Para detener:

```bash
docker compose down
```

---

# Comunicación Backend - Frontend

Los cuatro diseños mantienen una arquitectura de comunicación similar.

El backend funciona como servidor y establece una conexión mediante WebSockets con el frontend.

El frontend utiliza la API WebSocket del navegador:

```javascript
const socket = new WebSocket("ws://localhost:5000");
```

Cuando el backend actualiza la posición de un vehículo, envía sus datos al frontend. El frontend mantiene las posiciones recibidas y actualiza la posición visual de los vehículos dentro del juego. De esta forma, la principal diferencia entre los cuatro diseños está en **cómo el backend distribuye el trabajo de actualización entre los hilos**.

La comunicación entre backend y frontend permanece conceptualmente igual para facilitar la comparación entre diseños.

---

# Datos compartidos y sincronización

Los diseños que utilizan múltiples hilos deben controlar el acceso a información compartida.

Un posible escenario de condición de carrera aparece cuando dos hilos intentan modificar simultáneamente el estado de un vehículo o acceder simultáneamente a recursos compartidos.

Para evitar estos problemas se utilizan mecanismos como:

```cpp
std::mutex
```

Para proteger regiones críticas.

También se utilizan:

```cpp
std::atomic
```

cuando una variable compartida puede manejarse mediante operaciones atómicas.

En el Diseño 4 se utiliza además:

```cpp
std::condition_variable
```

para coordinar los trabajadores con la cola de tareas.

---

# Estructuras de datos utilizadas

## Diseño 1

La información de los vehículos se maneja principalmente mediante:

```cpp
vector<Car>
```

Cada hilo trabaja con un vehículo determinado.

---

## Diseño 2

Se utiliza:

```cpp
vector<Car>
```

El único hilo recorre la colección y actualiza los vehículos.

---

## Diseño 3

Se utiliza una estructura de agrupación como:

```cpp
CarGroup
```

Los vehículos se organizan de acuerdo con su tipo o color.

---

## Diseño 4

Se utilizan principalmente:

```cpp
vector<Car>
```

para almacenar los vehículos y:

```cpp
queue<function<void()>>
```

para almacenar las tareas pendientes de ejecución.

---

# Conclusión

El proyecto permite comparar cuatro estrategias diferentes para resolver un mismo problema de actualización de vehículos dentro de una simulación.

El **Diseño 1** maximiza la independencia entre vehículos, pero puede generar un número elevado de hilos cuando aumenta la cantidad de vehículos.

El **Diseño 2** simplifica considerablemente la implementación, pero limita el paralelismo al utilizar un único hilo para las actualizaciones.

El **Diseño 3** representa una solución intermedia al agrupar los vehículos por tipo y utilizar un hilo para cada grupo.

El **Diseño 4** utiliza un grupo fijo de trabajadores y una cola de tareas, separando la cantidad de vehículos de la cantidad de hilos y permitiendo una distribución dinámica del trabajo.

La comparación de estos diseños permite observar que **un mayor número de hilos no necesariamente significa un mejor rendimiento**. La estrategia adecuada depende de la cantidad de trabajo, los recursos disponibles, el nivel de sincronización requerido y la facilidad de mantenimiento del sistema.

---

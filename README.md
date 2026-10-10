# Compilador Casero v1.0.0

Un compilador de propósito educativo para el lenguaje **Mx**, compilado completamente desde cero en C11 con arquitectura modular y soporte multiplataforma.

## 🚀 Características Principales

- **Lenguaje Mx Personalizado**: Sintaxis clara y expresiva basada en pseudocódigo estructurado
- **Arquitectura Modular**: Separación clara de responsabilidades (lexer → parser → semántica → código)
- **Multiplataforma**: Compilación y ejecución en macOS, Linux y Windows
- **x86-64 Assembly**: Genera código ASM optimizado para arquitectura x86-64
- **Gestión de Errores Robusta**: Reportes de error estructurados en todas las fases
- **Sistema de Símbolos Jerarquizado**: Soporte para scopes anidados y resolución de variables
- **Optimización de IR**: Eliminación de código muerto en el código intermedio
- **Archivos**: apertura en modos de lectura/escritura, lectura por línea o completa y escritura
- **Depuración interactiva**: ejecución paso a paso, continuación y breakpoints por línea
- **Excepciones**: bloques `Intentar`/`Capturar`, `Lanzar` y lectura del último mensaje
- **Operadores bitwise**: `&`, `|`, `^`, `~`, `<<`, `>>` y sus asignaciones compuestas

## 📋 Requisitos

### Mínimos
- **CMake** 3.10+
- **C11 Compiler**: GCC, Clang o MSVC
- **NASM** (Netwide Assembler)
- **make** o plataforma de build nativa

### Sistema Operativo
- macOS 10.15+
- Ubuntu 18.04+ / Debian 10+
- Windows 10+ (con MSVC o MinGW)

El backend genera x86-64. En equipos x86-64 se ejecuta de forma nativa; en
Apple Silicon se requiere Rosetta 2 para ejecutar los binarios generados.
TLS usa OpenSSL 1.1.1 o posterior; es una dependencia opcional y solo se enlaza
en programas Mx que solicitan funciones TLS o `HttpTLS`.

## 🔧 Compilación

### Opción 1: Script Automatizado
```bash
./build.sh                    # Compila e instala NASM si es necesario
```

### Opción 2: CMake Manual
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/bin/compilador --version
```

### Opción 3: PowerShell en Windows
```powershell
.\build.ps1
```

## 📖 Uso Básico

### Compilar un programa Mx
```bash
./build/bin/compilador programa.mx programa_compilado
```

### Opciones de Compilación
```bash
./build/bin/compilador archivo.mx [salida] [opciones]

Opciones:
  -asm          conserva el archivo .asm generado
  -debug        muestra AST, tabla de símbolos, IR y ASM
  -debugger     pausa en cada sentencia; comandos s, c, b LINEA, q
  --diagnostics=json valida el archivo y emite diagnosticos estructurados
  -no-run       compila sin ejecutar el ejecutable
  -version      muestra la versión del compilador
  -help         muestra esta ayuda
```

### Ejemplo Completo
```bash
# Compilar solo
./build/bin/compilador tests/ejemplo1.mx mi_programa -no-run

# Compilar con debug
./build/bin/compilador tests/ejemplo1.mx mi_programa -debug

# Conservar el ASM generado
./build/bin/compilador tests/ejemplo1.mx mi_programa -asm

# Depurar interactivamente (s=paso, c=continuar, b 12=breakpoint, q=salir)
./build/bin/compilador tests/ejemplo1.mx mi_programa -debugger

# Obtener diagnosticos JSON para integracion con IDE
./build/bin/compilador archivo.mx --diagnostics=json
```

`--diagnostics=json` ejecuta el lexer, parser y analisis semantico sin generar
ni ejecutar un binario. Imprime un unico objeto JSON por `stdout`, incluso
cuando no hay errores:

```json
{
  "file": "archivo.mx",
  "diagnostics": [{
    "range": {
      "start": { "line": 2, "character": 8 },
      "end": { "line": 2, "character": 9 }
    },
    "severity": 1,
    "code": "MX3000",
    "source": "semantic",
    "message": "Variable no declarada: 'total'"
  }]
}
```

Las posiciones de `range` usan indices desde cero; `severity` sigue la
convencion LSP (`1` error, `2` advertencia, `3` informacion, `4` sugerencia).
Los codigos `MX1xxx`, `MX2xxx` y `MX3xxx` identifican diagnosticos del lexer,
parser y analisis semantico. `stdout` queda reservado para el JSON; los
registros generales del compilador siguen en `stderr`.

### Bibliotecas propias
Un programa puede incorporar otro archivo Mx con una ruta relativa al archivo
fuente que contiene la importación:
```mx
Importar "lib/matematicas.mx";
Mostrar(doblar(21));
```
Las funciones y tipos declarados en la biblioteca quedan disponibles como si
estuvieran en el programa principal. Las importaciones se expanden
recursivamente; no se permiten ciclos y la profundidad máxima es 64 archivos.

Ejemplos adicionales:

- `tests/lib_matematicas.mx`: cuadrados, cubos y porcentajes.
- `tests/lib_texto.mx`: texto y procedimientos `Void`.
- `tests/lib_datos.mx`: tipo personalizado `Medicion`.
- `tests/lib_inventario.mx`: array de existencias.
- `tests/usar_matematicas.mx`, `tests/usar_texto.mx` y
  `tests/usar_datos_inventario.mx`: programas consumidores.

## 🧪 Ejecución de Pruebas

```bash
# Ejecutar todos los programas de prueba
./run_tests.sh

# Ejecutar prueba individual
./build/bin/compilador tests/ejemplo1.mx -no-run
```

### Programas de Prueba Incluidos
- `adivinanza.mx` - Juego interactivo de adivinanza
- `calculadora.mx` - Calculadora básica
- `condicionales.mx` - Demostración de if/else
- `ejemplo1.mx` - Lectura y escritura I/O
- `fibonacci.mx` - Secuencia de Fibonacci
- `palindromo.mx` - Validación de palíndromos
- `paridad.mx` - Detección de números pares/impares
- `programa.mx` - Cálculo de factorial
- `prueba.mx` - Suite de 10 pruebas unitarias
- `prueba_integral.mx` - Suite completa con 13 características
- `suma.mx` - Suma acumulativa
- `tablas.mx` - Tabla de multiplicación
- `temperatura.mx` - Cálculo de área y conversión
- `archivos_basico.mx` - Lectura, escritura, modos y cierre de archivos
- `operadores_nuevos.mx` - Bitwise, conversiones booleanas y excepciones

## 💡 Ejemplo de Programa Mx

```mx
// Cálculo del factorial
Entero n = 5;
Entero fact = 1;
Entero i;

Para(i = 1; i <= n; i = i + 1) {
    fact = fact * i;
}

Mostrar("Factorial de ", n, " es ", fact, "\n");
```

## 📚 Documentación Completa

- [ARCHITECTURE.md](./ARCHITECTURE.md) - Arquitectura interna del compilador
- [BUILDING.md](./BUILDING.md) - Guía detallada de compilación
- [TESTING.md](./TESTING.md) - Protocolo de pruebas
- [VERSION.md](./VERSION.md) - Información de la versión
- [CHANGELOG.md](./CHANGELOG.md) - Historial de cambios

## 🏗️ Estructura del Proyecto

```
.
├── src/
│   ├── main.c              # Punto de entrada principal
│   ├── lexer/              # Análisis léxico
│   ├── parser/             # Análisis sintáctico
│   ├── semantic/           # Análisis semántico
│   ├── codegen/            # Generación de código
│   ├── symbols/            # Tabla de símbolos
│   ├── utils/              # Utilidades (I/O, tipos)
│   ├── errors/             # Manejo de errores
│   └── include/            # Headers públicos
├── tests/                  # Programas de prueba Mx
├── build/                  # Artefactos de compilación
├── CMakeLists.txt          # Configuración CMake
├── build.sh               # Script de compilación (Unix)
├── build.ps1              # Script de compilación (Windows)
└── run_tests.sh           # Script de ejecución de pruebas
```

## 🔄 Pipeline de Compilación

```
programa.mx
    ↓
[LEXER] → Tokens
    ↓
[PARSER] → AST (Abstract Syntax Tree)
    ↓
[SEMANTIC ANALYSIS] → Validación de tipos
    ↓
[IR GENERATION] → Código intermedio (cuádruples)
    ↓
[CODE GENERATION] → ASM x86-64
    ↓
[NASM] → Código objeto (.o/.obj)
    ↓
[LINKER] → Ejecutable final
    ↓
[RUNTIME] → Salida
```

## 🐛 Soporte para Tipos de Datos

| Tipo | Descripción | Rango |
|------|-------------|-------|
| `Entero` | Entero de 64 bits | -2^63 a 2^63-1 |
| `Flotante` | Punto flotante (64-bit) | IEEE 754 |
| `Cadena` | Cadena de caracteres | Hasta 256 caracteres |
| `Booleano` | Valor lógico (0/1) | 0 o 1 |

Las condiciones aceptan `Booleano` y `Entero` al estilo C: cero es falso y
cualquier valor distinto de cero es verdadero. Al asignar un entero a un
booleano, el valor se normaliza a 0 o 1.

### Archivos, excepciones y operadores bitwise

Los archivos se representan mediante descriptores enteros. `AbrirArchivo`
acepta los modos `r`, `w`, `a`, `r+`, `w+` y `a+`; `r` abre solo para lectura,
`w` crea/trunca para escritura y `a` agrega al final. Las funciones disponibles:

```mx
Entero archivo = AbrirArchivo("datos.txt", "r");
Cadena linea = LeerLinea(archivo);
Booleano fin = FinArchivo(archivo);
CerrarArchivo(archivo);
```

`LeerTodoArchivo(handle)` lee desde la posición actual (hasta 255 bytes por el
límite actual de cadenas); `EscribirArchivo(handle, texto)` devuelve los bytes
escritos o `-1` en error. `AbrirArchivo` devuelve `-1` si falla, `CerrarArchivo`
devuelve `Falso` para un descriptor no válido o si el cierre falla, y
`EliminarArchivo(ruta)` elimina una ruta y devuelve un booleano. Los fallos de
archivo se reportan en `stderr` y quedan disponibles en `UltimoError()`.

Las excepciones explícitas usan `Intentar { ... } Capturar { ... }` y
`Lanzar("mensaje");`. La captura puede declarar una variable local de tipo
`Cadena` con `Capturar (error) { ... }`; esa variable solo existe dentro del
bloque de captura. `ObtenerMensaje(error)` devuelve el mensaje asociado a esa
captura. `UltimoError()` se conserva para código existente. Las excepciones no
capturadas se informan y terminan el programa con error. Actualmente las
excepciones exponen el mensaje, no un stack trace.

```mx
Intentar {
    Entero resultado = 10 / 0;
} Capturar (error) {
    Mostrar("No se pudo calcular: ", ObtenerMensaje(error), "\n");
}
```

Los operadores bitwise trabajan con `Entero` de 64 bits con signo, respetan la
precedencia habitual (`~`, desplazamientos, `&`, `^`, `|`) y admiten `&=`, `|=`,
`^=`, `<<=` y `>>=`. El contador de desplazamiento se reduce a los 6 bits bajos,
de acuerdo con el comportamiento de las instrucciones x86-64 del backend.

### Sockets TCP y UDP

Los sockets se representan mediante descriptores enteros y admiten hasta 128
conexiones abiertas a la vez. La API permite conectar clientes TCP, escuchar y
aceptar conexiones, enviar y recibir texto, usar UDP, establecer timeouts de
envio/recepcion y consultar direcciones remotas y puertos locales:

```mx
Entero udp = SocketUDP(0);
Entero puerto = SocketPuertoLocal(udp);
SocketTimeout(udp, 1000, 1000);
SocketEnviarUDP(udp, "127.0.0.1", puerto, "hola");
Cadena mensaje = SocketRecibirUDP(udp, 255);
Cadena remitente = SocketDireccionRemota(udp);
SocketCerrar(udp);
```

Funciones disponibles: `SocketTCPConectar(host, puerto)`,
`SocketTCPEscuchar(puerto, backlog)`, `SocketTCPAceptar(servidor)`,
`SocketEnviar(socket, texto)`, `SocketRecibir(socket, maxBytes)`,
`SocketRecibirLinea(socket, maxBytes)`,
`SocketUDP(puertoLocal)`, `SocketEnviarUDP(socket, host, puerto, texto)`,
`SocketRecibirUDP(socket, maxBytes)`, `SocketDireccionRemota(socket)`,
`SocketPuertoLocal(socket)`, `SocketTimeout(socket, recibirMs, enviarMs)` y
`SocketCerrar(socket)`. Un puerto local de `0` solicita un puerto efimero del
sistema; `SocketPuertoLocal` permite consultarlo. Los timeouts se expresan en
milisegundos; `0` los desactiva. `SocketRecibir` devuelve lo que haya disponible
hasta `maxBytes` (1..255), no garantiza un mensaje TCP completo; TCP es un
flujo, mientras que UDP conserva los limites de cada datagrama. La API es de
texto, no de datos binarios arbitrarios. Los fallos devuelven `-1`, una cadena
vacia o `Falso`, segun la operacion, y dejan detalles en `UltimoError()`.
`SocketRecibirLinea` espera hasta recibir un salto de linea y elimina `CR/LF`;
es útil para delimitar mensajes de chat sobre TCP.

#### Chat TCP interactivo de 16 mensajes

El ejemplo de chat funciona en dos terminales y muestra cada mensaje al
recibirlo. Compila ambos programas:

```bash
./build/bin/compilador examples/sockets_chat_servidor.mx /tmp/mx_chat_servidor -no-run
./build/bin/compilador examples/sockets_chat_cliente.mx /tmp/mx_chat_cliente -no-run
```

En una terminal inicia `/tmp/mx_chat_servidor`; en la otra, inicia
`/tmp/mx_chat_cliente`. El cliente envia el primer mensaje y luego los
participantes alternan. Los mensajes pueden incluir espacios. Al completar
16 mensajes, ambos lados cierran la conexion. El puerto predeterminado del
ejemplo es `5050`.

Ejemplo de servidor TCP de una conexion:

```mx
Entero servidor = SocketTCPEscuchar(5050, 8);
Entero cliente = SocketTCPAceptar(servidor);
SocketTimeout(cliente, 5000, 5000);
Cadena solicitud = SocketRecibir(cliente, 255);
SocketEnviar(cliente, "Recibido: ");
SocketEnviar(cliente, solicitud);
SocketCerrar(cliente);
SocketCerrar(servidor);
```

#### TLS y HTTPS

TLS 1.2 o posterior esta disponible para conexiones TCP existentes y
peticiones HTTPS. El cliente valida el certificado usando las autoridades
certificadoras configuradas para OpenSSL y comprueba el hostname; rechaza
certificados no confiables o emitidos para otro nombre. El servidor usa un
certificado y una clave privada en formato PEM:

```mx
Entero cliente = SocketTCPConectar("example.com", 443);
SocketTimeout(cliente, 5000, 5000);
Booleano seguro = SocketTLSCliente(cliente, "example.com");
Si (seguro) {
    SocketEnviar(cliente, "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n");
    Cadena respuesta = SocketRecibir(cliente, 255);
    Mostrar(respuesta);
}
SocketCerrar(cliente);

Entero servidorTLS = SocketTCPEscuchar(5443, 8);
SocketTLSServidor(servidorTLS, "certificado.pem", "clave-privada.pem");
Entero conexion = SocketTCPAceptar(servidorTLS);
SocketEnviar(conexion, "conexion cifrada");
SocketCerrar(conexion);
SocketCerrar(servidorTLS);

Entero estado = HttpTLS("https://example.com/", "GET", "", "");
Cadena cuerpo = HttpCuerpo();
```

`SocketTLSServidor` configura el listener y `SocketTCPAceptar` completa el
handshake antes de devolver la conexion. `SocketEnviar` y `SocketRecibir`
usan TLS automaticamente despues del handshake. `HttpTLS` requiere una URL
`https://`; `Http` mantiene su compatibilidad y cifra tambien las URLs HTTPS
literales que el compilador puede identificar. Instala OpenSSL antes de
configurar CMake para habilitar estas funciones. Sin OpenSSL, los programas
que no solicitan TLS siguen compilando; al pedir TLS el compilador informa
que falta la dependencia.

## 🔐 Operadores Soportados

### Aritméticos
- `+` Suma
- `-` Resta
- `*` Multiplicación
- `/` División entera
- `%` Módulo

### Comparación
- `==` Igual
- `!=` No igual
- `<` Menor que
- `>` Mayor que
- `<=` Menor o igual
- `>=` Mayor o igual

### Lógicos
- `&&` AND lógico
- `|` OR lógico
- `No` NOT lógico

## 📝 Declaraciones Soportadas

- `Entero`, `Flotante`, `Cadena` - Declaración de variables
- Arrays de tamaño fijo con índices constantes, por ejemplo `Entero nums[3] = {10, 20, 30};` y `Mostrar(nums[1]);`
- `Si`, `Sino` - Condicionales
- `Para` - Bucle for
- `Mientras` - Bucle while
- `Romper` - Break
- `Continuar` - Continue
- `Mostrar` - Salida (printf)
- `Leer` - Entrada (scanf)

Los arrays se almacenan como elementos independientes y actualmente requieren un tamaño entero positivo e índices constantes.

Las funciones usan retorno tipado y parámetros por valor:
```mx
Entero sumar(Entero a, Entero b) {
    Retornar a + b;
}
Void saludar() {
    Mostrar("Hola\n");
}
```

También se pueden declarar tipos personalizados y variables de esos tipos:
```mx
Tipo Persona { Entero edad; };
Persona p;
```
Los campos pueden leerse y asignarse con notación de punto:
```mx
p.edad = 35;
Mostrar(p.edad);
```
Internamente los campos se almacenan como miembros planos, por lo que actualmente se admiten campos de tipos primitivos.

### Funciones nativas

Mx incluye funciones nativas para no tener que reimplementar operaciones comunes:

| Grupo | Funciones |
|------|-----------|
| Enteros | `Abs`, `Absoluto`, `Min`, `Max` |
| Aleatoriedad | `Aleatorio()`, `Aleatorio(max)`, `AleatorioEntre(min, max)` |
| Trigonometría | `Seno`, `Coseno`, `Tangente` |
| Potencias y logaritmos | `Potencia`, `RaizCuadrada`, `Logaritmo`, `Exponencial` |
| Redondeo | `Piso`, `Techo`, `Redondear` |
| Cadenas | `Longitud`, `Comparar`, `Contiene` |

Ejemplo:
```mx
Flotante x = Potencia(2.0, 8.0);
Cadena mensaje = "Hola Mx";
Mostrar(x, " ", Longitud(mensaje), "\n");
```

Las funciones numéricas aceptan los tipos indicados por su firma; las funciones
de cadenas reciben `Cadena`. Los errores de cantidad o tipo de argumentos se
reportan durante el análisis semántico.

`Aleatorio(max)` devuelve un entero entre `0` y `max - 1`.
`AleatorioEntre(min, max)` devuelve un entero dentro del rango inclusivo.
Ambas usan el generador de la biblioteca estándar del sistema.

### Configuración `.env`

Si existe un archivo `.env` junto al programa fuente, el compilador carga sus
pares `CLAVE=VALOR` antes de compilar. Las variables ya definidas en el sistema
tienen prioridad. Desde Mx se consultan así:

```mx
Cadena url = Entorno("API_URL");
Si (ExisteEntorno("API_URL")) {
    Mostrar(url, "\n");
}
```

El archivo `.env` no se incluye automáticamente en el ejecutable ni debe
versionarse si contiene credenciales.

### HTTP con sockets del sistema

Mx incluye una primera capa HTTP mediante sockets del sistema en
`runtime/mx_runtime.c`:

```mx
Cadena url = Entorno("API_URL");
Entero estado = Http(url, "GET", "Accept: application/json", "");
Cadena cuerpo = HttpCuerpo();
Mostrar("Estado: ", estado, "\n");
Mostrar(cuerpo);
```

`Http(url, metodo, cabeceras, cuerpo)` acepta cualquier método HTTP, cabeceras
separadas por saltos de línea y un cuerpo opcional. La respuesta de la última
petición se consulta por partes:

```mx
Cadena cabeceras = HttpCabeceras();
Cadena cuerpo = HttpCuerpo();
Mostrar("Cabeceras: ", cabeceras, "\n");
Mostrar("Cuerpo: ", cuerpo, "\n");
```

`HttpCuerpo()` devuelve el texto completo de la respuesta (por ejemplo, JSON)
hasta 1 MiB. `HttpCabeceras()` devuelve las cabeceras de respuesta separadas
por saltos de línea. La implementación usa
HTTP/1.1 sobre TCP y resolución DNS del sistema, sin depender de `curl`.
Las URL `https://` requieren que el programa se enlace con OpenSSL 1.1.1+
como se describe en la sección de TLS. Usa `HttpTLS` cuando la URL se obtiene
dinámicamente; una URL HTTPS literal pasada a `Http` también habilita TLS.
La prueba mínima está en `tests/http.mx`.

## 🛠️ Mantenimiento

### Limpiar Artefactos de Compilación
```bash
rm -rf build/
rm -f *.asm *.o *.obj *.exe
```

### Regenerar Compilación
```bash
./build.sh
```

## 📄 Licencia

Proyecto educativo - Uso libre para fines académicos.

## 👤 Autor

Carlos Sandoval - 2026

## 🤝 Contribuciones

Este es un proyecto educativo. Se aceptan sugerencias y mejoras.

## ✅ Estado de Proyecto

**Versión 1.0.0** - STABLE

- ✅ Fase 1: Análisis léxico y sintáctico
- ✅ Fase 2: Análisis semántico
- ✅ Fase 3: Generación de código IR
- ✅ Fase 4: Compilación a ASM x86-64
- ✅ Fase 5: Enlazado a ejecutables
- ✅ Fase 6: Suite de pruebas de `tests/*.mx`
- ✅ Fase 7: Documentación completa
- ✅ Fase 8: Optimización y limpieza

---

**Última actualización**: Agosto 2026

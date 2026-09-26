# Visión Artificial: calibración, pose y conteo de sentadillas

Aplicación integrada en C++17 para calibrar cámaras con un tablero de ajedrez,
estimar pose humana con MediaPipe Pose Landmarker y contar sentadillas a partir
de los landmarks de cadera, rodilla y tobillo.

El sistema admite archivos de video y cámaras conectadas, puede corregir la
distorsión utilizando una calibración previa y permite guardar videos anotados
con los landmarks y las métricas calculadas.

## Arquitectura

```text
Vision_Artificial/
├── BUILD
├── bin/
│   └── vision_app              # ejecutable compilado para Linux x86_64
├── include/
│   ├── calibration.h
│   ├── pose_analysis.h
│   └── squat_counter.h
├── src/
│   ├── main.cpp
│   ├── calibration.cpp
│   ├── pose_analysis.cpp
│   └── squat_counter.cpp
├── data/
│   ├── calibration/
│   │   ├── iphone_images/
│   │   └── tablet_images/
│   └── videos/
├── models/
│   └── pose_landmarker.task
├── results/
│   ├── calibration/
│   └── pose/
└── Libros/
```

## Flujo de la aplicación

1. `calibrate` enumera las imágenes compatibles de un directorio, detecta las
   esquinas internas del tablero de calibración, refina su posición a precisión
   subpíxel y calcula la matriz intrínseca, los coeficientes de distorsión y los
   errores de reproyección.

2. `pose` abre una cámara o un archivo de video. Si se proporciona un archivo
   YAML de calibración, cada frame se rectifica antes de realizar la inferencia.

3. MediaPipe Pose Landmarker estima 33 landmarks corporales normalizados. El
   pipeline dibuja el esqueleto y utiliza los landmarks de cadera, rodilla y
   tobillo para calcular los ángulos articulares.

4. El contador utiliza histéresis para detectar repeticiones: entra en fase
   baja cuando el ángulo de rodilla es inferior a 120° y cuenta una repetición
   cuando vuelve a superar 155°. Los landmarks utilizados deben alcanzar una
   presencia y visibilidad mínimas de 0.5.

La histéresis y los umbrales de 120°/155° se adaptaron del enfoque de
`AngleRepCounter` de
[Shalbulov/exercise_counter](https://github.com/Shalbulov/exercise_counter),
publicado bajo licencia MIT.

La adaptación utiliza los índices de landmarks de MediaPipe correspondientes
a caderas, rodillas y tobillos, mantiene estado independiente para ambos lados
del cuerpo y utiliza el máximo de los contadores izquierdo y derecho para
tolerar oclusiones parciales.

## Entorno de compilación validado

El proyecto fue compilado y ejecutado satisfactoriamente en Linux x86_64 bajo
WSL con la siguiente combinación:

| Componente | Versión validada |
| --- | --- |
| Sistema | Ubuntu 22.04 bajo WSL |
| C++ | C++17 |
| MediaPipe | commit `6756ba4d59bdd8ac44173df8d4ab4069c0a747ec` |
| Bazel | `7.7.0` |
| GCC | `13.4.0` |
| G++ | `13.4.0` |
| GNU Binutils | `2.40` |
| Java | OpenJDK `17.0.20.1` |
| OpenCV | `4.5.4` |
| FFmpeg | `4.4.2` |
| Python hermético de Bazel | `3.11` |
| MediaPipe delegate | CPU |

Esta combinación se documenta porque las dependencias C++ de MediaPipe,
Protobuf, Abseil, TensorFlow Lite y XNNPACK pueden ser sensibles a las versiones
del compilador, assembler y bibliotecas del sistema.

### MediaPipe

Se utilizó el siguiente commit:

```bash
git clone https://github.com/google-ai-edge/mediapipe.git
cd mediapipe

git checkout 6756ba4d59bdd8ac44173df8d4ab4069c0a747ec

echo "7.7.0" > .bazelversion
export USE_BAZEL_VERSION=7.7.0
export HERMETIC_PYTHON_VERSION=3.11
```

### OpenCV 4

En Ubuntu 22.04, OpenCV 4.5.4 se encuentra normalmente en:

```text
/usr/include/opencv4
```

Para utilizar OpenCV 4 con este checkout de MediaPipe fue necesario habilitar
las entradas correspondientes en:

```text
mediapipe/third_party/opencv_linux.BUILD
```

incluyendo:

```text
include/x86_64-linux-gnu/opencv4/opencv2/cvconfig.h
include/opencv4/opencv2/**/*.h*
```

y las rutas:

```text
include/x86_64-linux-gnu/opencv4/
include/opencv4/
```

### GCC y Binutils

La compilación utiliza GCC/G++ 13.4.0.

También se utilizó GNU Binutils 2.40. Una versión anterior del assembler
(Binutils 2.38) no reconocía algunas instrucciones utilizadas por los
microkernels de XNNPACK, entre ellas `vpdpbssd`.

Durante la compilación se proporcionó explícitamente la ubicación de Binutils
2.40 a GCC y Bazel.

### Compilación

El proyecto se compila desde el checkout de MediaPipe, indicando mediante
`--package_path` el directorio que contiene `Vision_Artificial`.

Ejemplo correspondiente al entorno validado:

```bash
cd ~/mediapipe

export USE_BAZEL_VERSION=7.7.0
export HERMETIC_PYTHON_VERSION=3.11
export CC=/usr/bin/gcc-13
export CXX=/usr/bin/g++-13

export NEW_BINUTILS="$HOME/opt/binutils-2.40/bin"
export PATH="$NEW_BINUTILS:$HOME/bin:$PATH"

bazel build \
  -c opt \
  --define MEDIAPIPE_DISABLE_GPU=1 \
  --jobs=1 \
  --repo_env=CC=/usr/bin/gcc-13 \
  --repo_env=CXX=/usr/bin/g++-13 \
  --action_env=PATH="$PATH" \
  --host_action_env=PATH="$PATH" \
  --copt="-B$NEW_BINUTILS/" \
  --host_copt="-B$NEW_BINUTILS/" \
  --linkopt="-B$NEW_BINUTILS/" \
  --host_linkopt="-B$NEW_BINUTILS/" \
  --package_path="$PWD:$HOME/Tareas/Tareas_2026/Octavo_Semestre" \
  //Vision_Artificial:vision_app
```

El ejecutable generado por Bazel se encuentra en:

```text
~/mediapipe/bazel-bin/Vision_Artificial/vision_app
```

En este repositorio se almacena una copia en:

```text
bin/vision_app
```

## Ejecutable incluido

El ejecutable incluido en `bin/vision_app` fue compilado para Linux x86_64 en
el entorno descrito anteriormente.

Puede ejecutarse directamente desde la raíz del repositorio:

```bash
./bin/vision_app --help
```

El binario utiliza bibliotecas dinámicas del sistema y, por tanto, no se
garantiza su portabilidad a sistemas operativos, arquitecturas o distribuciones
Linux diferentes.

Para máxima reproducibilidad se recomienda compilar el código fuente en el
sistema donde vaya a utilizarse.

## Uso

Todos los siguientes comandos se ejecutan desde la raíz de
`Vision_Artificial`.

### Calibración de cámara

Ejemplo utilizando un tablero de 8 × 5 esquinas internas y cuadros de 26 mm:

```bash
./bin/vision_app calibrate \
  --images data/calibration/iphone_images \
  --output results/runtime/iphone \
  --board-cols 8 \
  --board-rows 5 \
  --square-mm 26
```

La calibración genera un archivo YAML con la matriz intrínseca y los
coeficientes de distorsión.

### Pose sobre un video

```bash
./bin/vision_app pose \
  --input data/videos/walking.mp4 \
  --model models/pose_landmarker.task
```

### Pose con corrección de distorsión

```bash
./bin/vision_app pose \
  --input data/videos/walking.mp4 \
  --model models/pose_landmarker.task \
  --calibration results/calibration/iphone_images/calibration_results.yaml
```

### Guardar video anotado

```bash
./bin/vision_app pose \
  --input data/videos/walking.mp4 \
  --model models/pose_landmarker.task \
  --output results/runtime/walking_annotated.mp4
```

### Cámara en tiempo real

Cámara predeterminada:

```bash
./bin/vision_app pose \
  --input camera \
  --model models/pose_landmarker.task
```

Cámara seleccionada por índice:

```bash
./bin/vision_app pose \
  --input camera:0 \
  --model models/pose_landmarker.task
```

o:

```bash
./bin/vision_app pose \
  --input camera:1 \
  --model models/pose_landmarker.task
```

En Linux, la cámara debe estar disponible como dispositivo de video, por
ejemplo `/dev/video0`.

Cuando la aplicación se ejecuta dentro de WSL, la webcam de Windows puede no
estar expuesta automáticamente como dispositivo V4L2. En ese caso es necesario
habilitar el acceso del dispositivo desde WSL antes de utilizar `camera`.

### Controles

Durante la visualización interactiva:

- `Espacio`: pausar o reanudar.
- `R`: reiniciar el contador.
- `Q`: terminar.
- `Esc`: terminar.

### Ejecución sin interfaz gráfica

Para procesar un archivo sin abrir una ventana:

```bash
./bin/vision_app pose \
  --input data/videos/walking.mp4 \
  --model models/pose_landmarker.task \
  --output results/runtime/walking_annotated.mp4 \
  --no-display
```

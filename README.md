# Visión Artificial: calibración, pose y conteo de sentadillas

Aplicación integrada en C++17 para calibrar cámaras con un tablero de ajedrez,
estimar pose con MediaPipe Pose Landmarker y contar sentadillas a partir de los
landmarks de cadera, rodilla y tobillo. Admite archivos de video y cámaras
conectadas, puede corregir la distorsión con una calibración previa y permite
guardar el video anotado.

## Arquitectura

```text
Vision_Artificial/
├── BUILD                       # configuración Bazel para MediaPipe
├── include/
│   ├── calibration.h           # API del calibrador
│   ├── pose_analysis.h         # API del pipeline de pose
│   └── squat_counter.h         # contador y umbrales
├── src/
│   ├── main.cpp                # CLI integrada
│   ├── calibration.cpp         # detección, calibración y error
│   ├── pose_analysis.cpp       # cámara/video, MediaPipe y visualización
│   └── squat_counter.cpp       # ángulos de rodilla e histéresis
├── data/
│   ├── calibration/
│   │   ├── iphone_images/
│   │   └── tablet_images/
│   └── videos/
├── models/
│   └── pose_landmarker.task    # modelo requerido por MediaPipe
├── results/
│   ├── calibration/
│   └── pose/
└── Libros/
```

## Flujo de la aplicación

1. `calibrate` enumera todas las imágenes compatibles del directorio, detecta
   las esquinas internas, refina su posición a precisión subpíxel y calcula
   matriz intrínseca, distorsión y errores de reproyección.
2. `pose` abre una cámara o un video. Si recibe un YAML de calibración,
   rectifica cada frame antes de la inferencia.
3. MediaPipe Pose Landmarker produce 33 landmarks normalizados. El pipeline
   dibuja el esqueleto y entrega caderas, rodillas y tobillos al contador.
4. El contador calcula el ángulo cadera-rodilla-tobillo de cada lado y usa
   histéresis: entra en fase baja por debajo de 120° y cuenta al volver a
   superar 155°. Los landmarks deben tener presencia y visibilidad de al
   menos 0.5.

La histéresis y los umbrales de 120°/155° se adaptaron del enfoque de
`AngleRepCounter` de
[Shalbulov/exercise_counter](https://github.com/Shalbulov/exercise_counter),
publicado bajo licencia MIT. La adaptación reemplaza los 17 puntos COCO/YOLO
por los índices MediaPipe 23–28, mantiene estado independiente por lado y usa
el máximo de ambos contadores para tolerar oclusiones.

## Entorno de compilación validado

El proyecto utiliza Bazel y las dependencias C++ de MediaPipe. Debido a que
MediaPipe incorpora un árbol amplio de dependencias —entre ellas Protobuf,
Abseil, XNNPACK y TensorFlow Lite— la compatibilidad entre compilador, linker,
bibliotecas del sistema y versiones de las dependencias es importante.

La siguiente combinación fue validada compilando y ejecutando correctamente
el pipeline completo:

| Componente | Versión validada |
| --- | --- |
| MediaPipe | commit `6756ba4d59bdd8ac44173df8d4ab4069c0a747ec` |
| Bazel | `7.7.0` |
| C++ | C++17 |
| GCC | `13.1.0` |
| GNU Binutils / `ld` | `2.40` |
| glibc | `2.28` |
| OpenCV | `4.13.0` |
| FFmpeg | `8.1.2` |
| libstdc++ | `16.2.0` |
| libgcc | `16.2.0` |
| Java | `19.0.2` |
| Python hermético de Bazel | `3.11` |
| MediaPipe delegate | CPU |

La versión de MediaPipe puede fijarse explícitamente con:

```bash
git clone https://github.com/google-ai-edge/mediapipe.git
cd mediapipe

git checkout 6756ba4d59bdd8ac44173df8d4ab4069c0a747ec

echo "7.7.0" > .bazelversion
export USE_BAZEL_VERSION=7.7.0
export HERMETIC_PYTHON_VERSION=3.11
```

El proyecto debe compilarse desde el checkout de MediaPipe, añadiendo mediante
`--package_path` el directorio que contiene `Vision_Artificial`. De forma
general:

```bash
cd /ruta/a/mediapipe

export USE_BAZEL_VERSION=7.7.0
export HERMETIC_PYTHON_VERSION=3.11

bazel build \
  -c opt \
  --define MEDIAPIPE_DISABLE_GPU=1 \
  --package_path="$PWD:/ruta/al/directorio/que/contiene/el/proyecto" \
  //Vision_Artificial:vision_app
```

La ubicación de OpenCV y FFmpeg depende de la distribución de Linux y del
método de instalación. En sistemas donde OpenCV 4 está instalado mediante el
gestor de paquetes, los headers suelen encontrarse en
`/usr/include/opencv4`. En ese caso puede ser necesario habilitar las rutas
correspondientes dentro de `third_party/opencv_linux.BUILD` de MediaPipe.

Los ajustes específicos del linker y las rutas absolutas de bibliotecas
dependen del sistema donde se compile y, por tanto, no se incluyen en el
repositorio. En particular, una compilación realizada en un sistema puede
quedar enlazada dinámicamente contra sus versiones locales de `glibc`,
`libstdc++`, OpenCV y otras bibliotecas; por esta razón el ejecutable generado
no se considera portable entre máquinas diferentes.

No se incluyen binarios compilados en el repositorio. Para reproducir el
proyecto se recomienda compilar el ejecutable directamente en el sistema donde
será utilizado.

## Uso

Calibrar con las imágenes de iPhone (tablero de 8 × 5 esquinas internas y
cuadros de 26 mm):

```bash
./bazel-bin/Vision_Artificial/vision_app calibrate \
  --images data/calibration/iphone_images \
  --output results/runtime/iphone \
  --board-cols 8 \
  --board-rows 5 \
  --square-mm 26
```

Analizar un video con corrección de distorsión:

```bash
./bazel-bin/Vision_Artificial/vision_app pose \
  --input data/videos/walking.mp4 \
  --model models/pose_landmarker.task \
  --calibration results/calibration/iphone_images/calibration_results.yaml \
  --output results/runtime/walking_annotated.mp4
```

Analizar un video sin aplicar calibración:

```bash
./bazel-bin/Vision_Artificial/vision_app pose \
  --input data/videos/lunges.mp4 \
  --model models/pose_landmarker.task \
  --output results/runtime/lunges_annotated.mp4
```

Usar la cámara predeterminada o una cámara concreta:

```bash
./bazel-bin/Vision_Artificial/vision_app pose \
  --input camera \
  --model models/pose_landmarker.task

./bazel-bin/Vision_Artificial/vision_app pose \
  --input camera:1 \
  --model models/pose_landmarker.task
```

Opciones de interacción: espacio pausa/reanuda, `R` reinicia el contador y
`Q` o `Esc` termina.

Para ejecución sin interfaz gráfica use `--no-display` junto con `--output`:

```bash
./bazel-bin/Vision_Artificial/vision_app pose \
  --input data/videos/lunges.mp4 \
  --model models/pose_landmarker.task \
  --output results/runtime/lunges_annotated.mp4 \
  --no-display
```

## Validación de ejecución

La configuración anterior fue probada ejecutando el pipeline completo sobre un
archivo de video. El flujo validado comprende:

```text
Video
  ↓
OpenCV VideoCapture
  ↓
MediaPipe Pose Landmarker
  ↓
33 landmarks corporales
  ↓
Cálculo de ángulos de rodilla
  ↓
Conteo de repeticiones
  ↓
Visualización de landmarks y métricas
  ↓
Video anotado
```

El procesamiento puede realizarse utilizando exclusivamente CPU. El modelo
`pose_landmarker.task` debe estar disponible localmente y suministrarse con
`--model` cuando no se utilice la ruta predeterminada.

## Limitaciones actuales

El contador actual identifica repeticiones principalmente a partir de los
ángulos de flexión y extensión de las rodillas. Por esta razón, otros
movimientos que produzcan patrones angulares similares, como lunges, pueden
ser contabilizados como sentadillas.

Esta implementación constituye una primera etapa del sistema y deberá
complementarse con criterios espaciales, temporales y biomecánicos adicionales
para distinguir tipos de ejercicio y evaluar posteriormente características
como simetría, calidad de ejecución y evolución longitudinal.

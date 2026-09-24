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

`data/` y `models/` se versionan intencionalmente. `Libros/`, los binarios,
las salidas de Bazel, los directorios de compilación y las ejecuciones nuevas
en `results/runtime/` o `results/generated/` se ignoran.

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

## Compilación en el HPC

El proyecto debe compilarse desde el checkout de MediaPipe, añadiendo el
directorio que contiene `Vision_Artificial` al `package_path`. Ejemplo
orientativo (ajuste las rutas y la versión hermética de Python del clúster):

```bash
cd /ruta/a/mediapipe
export HERMETIC_PYTHON_VERSION=3.12
bazel build -c opt \
  --define MEDIAPIPE_DISABLE_GPU=1 \
  --package_path="$PWD:/ruta/al/directorio/que/contiene/el/proyecto" \
  //Vision_Artificial:vision_app
```

No se incluyen binarios compilados en el repositorio.

## Uso

Calibrar con las imágenes de iPhone (tablero de 8 × 5 esquinas internas y
cuadros de 26 mm):

```bash
./bazel-bin/Vision_Artificial/vision_app calibrate \
  --images data/calibration/iphone_images \
  --output results/runtime/iphone \
  --board-cols 8 --board-rows 5 --square-mm 26
```

Analizar un video con corrección de distorsión:

```bash
./bazel-bin/Vision_Artificial/vision_app pose \
  --input data/videos/walking.mp4 \
  --model models/pose_landmarker.task \
  --calibration results/calibration/iphone_images/calibration_results.yaml \
  --output results/runtime/walking_annotated.mp4
```

Usar la cámara predeterminada o una cámara concreta:

```bash
./bazel-bin/Vision_Artificial/vision_app pose --input camera
./bazel-bin/Vision_Artificial/vision_app pose --input camera:1
```

Opciones de interacción: espacio pausa/reanuda, `R` reinicia el contador y
`Q` o `Esc` termina. Para ejecución sin interfaz gráfica use `--no-display`
junto con `--output`.


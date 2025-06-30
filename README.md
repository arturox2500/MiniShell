# MiniShell – Sistemas Operativos

## 📌 Descripción

Este proyecto se desarrolló como parte de la asignatura **Sistemas Operativos** del Grado en Ingeniería del Software. Consiste en la implementación de un **intérprete de comandos (MiniShell)** en lenguaje **C** para entornos **Linux**. La shell es capaz de:

- Ejecutar mandatos secuenciales y en pipeline (`|`)
- Soportar redirecciones:
  - Entrada estándar: `< fichero`
  - Salida estándar: `> fichero`
  - Salida de error: `>& fichero`
- Ejecutar comandos en segundo plano (background) mediante `&`
- Gestionar múltiples procesos (hasta 20 simultáneamente) en **fg** y **bg**
- Controlar la ejecución de procesos mediante las señales del teclado:
  - `Ctrl+C` (`SIGINT`): interrumpe el proceso en fg sin afectar la MiniShell
  - `Ctrl+Z` (`SIGTSTP`): pausa el proceso en fg sin detener la MiniShell
- Control de errores robusto en la ejecución de mandatos, redirecciones y uso de señales.

---

## 🛠️ Pasos para ejecutar

1. Descarga el fichero fuente en un sistema **Linux**.
2. Descomprime el proyecto si está comprimido.
3. Abre una terminal.
4. Compila el archivo con:

   ```bash
   gcc -Wall -o minishell miniShell.c

4. Ejecuta la MiniShell:

   ```bash
   ./minishell

### Para salir, puedes:

- Escribir `exit`, o  
- Pulsar `Ctrl+D` dos veces.

---

## 👨‍💻 Comandos internos implementados

- `cd [ruta]`: Cambia de directorio. Si no se especifica ruta, va al directorio `$HOME`.
- `jobs`: Muestra los trabajos en segundo plano o detenidos.
- `bg [n]`: Reanuda en segundo plano el trabajo detenido número *n*.
- `fg [n]`: Reanuda en primer plano el trabajo detenido número *n*.
- `umask [valor]`: Establece la máscara de permisos. Si se ejecuta sin argumentos, muestra la actual.
- `exit`: Sale de la MiniShell.

## 👥 Colaboradores

- **Arturo Enrique Gutiérrez Mirandona**  
- **Iván Gutiérrez González**

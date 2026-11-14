# RegistryExplorer++

> Inferido del brief explícito del usuario; no arranca de una entrevista.
> Marca y mundo visual heredados de jumplens (mismo producto familiar, misma paleta, misma tabla).

## Qué es

Explorador offline de hives del registro de Windows, reescrito en C++20 con Dear ImGui y
DirectX 11. Toma el trabajo de Eric Zimmerman como referencia algorítmica (su parser
`Registry`, MIT) y lo extiende: UI nativa, exportación CSV, filtrado y búsqueda potente,
y una experiencia de datos densa pensada para análisis forense rápido.

## Para quién

Analistas forenses y operadores de máquinas Windows (incluido el ecosistema del autor,
Silence AC). Personas que abren un hive offline y quieren responder "¿hay algo sospechoso
acá?" en segundos.

## Qué compite con qué

- Microsoft Register Editor (regedit): consola en vivo, sin análisis offline, sin exportación
  masiva, sin filtrado.
- Eric Zimmerman RegistryExplorer / RECmd: el estado del arte — C#, cerrado el primero,
  CLI el segundo. Nosotros hacemos la versión abierta, moderna, más rápida y más capaz.

## Qué tiene que ganar

- Abrir hives corruptos/parciales SIN morir: datos parciales + notas, no un fail.
- Una tabla de valores tan buena como la de jumplens: búsqueda instant, sort, filtros,
  selección múltiple, atajos de teclado.
- Diffing entre dos hives en el mismo árbol (doble fuente) como feature insignia.
- Exportar selección o hive entero a CSV/JSON.

## Qué NO es

No toca el registro en vivo jamás. No es un editor (solo lectura por defecto; escritura es
EXPERIMENTAL y explícita). No es ante-window: es una herramienta de análisis.

## Restricciones reales

- C++20, MSVC, Windows SDK 10.0.26100. Sin runtime, sin instalador, un .exe.
- Dear ImGui (docking branch) vendorizado; stb_image para texturas.
- Paleta jumplens: sumi #0B0D0E, panel #101314, amber #E8A33C, washi #E9E7E2.
- "Made by diquuuu" en la UI y el README (regla del autor).
- Sin wordmark inventado: logo = Phosphor (MIT), dominio: llave + registro.
- MIT license con crédito a Eric Zimmerman por el trabajo de referencia.

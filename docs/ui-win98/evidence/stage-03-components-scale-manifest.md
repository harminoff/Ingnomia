# Stage 03 component gallery scale captures

Captured 2026-09-24 from the production `Ingnomia.exe` in `RelWithDebInfo`, using the component-fixture automation and isolated temporary data folder `Ingnomia-Win98-Stage03-runtime-check`. The window/context was 3231×1631 physical pixels. Fixture content is synthetic; no save was opened.

| UI scale | Gallery trace | Layout measurements | Capture |
|---:|---|---|---|
| 100% | [trace](stage-03-components-scale-100.trace.txt) | root 3231×1631; body 3199×1599; header 2968×108; grid 3000×1011; `layout=pass` | [capture](stage-03-components-scale-100.png) |
| 125% | [trace](stage-03-components-scale-125.trace.txt) | root 3231×1631; body 3191×1591; header 3085×134; grid 3125×1263; `layout=pass` | [capture](stage-03-components-scale-125.png) |
| 150% | [trace](stage-03-components-scale-150.trace.txt) | root 3231×1631; body 3183×1583; header 3027×164; grid 3075×1548; `layout=pass` | [capture](stage-03-components-scale-150.png) |
| 200% | [trace](stage-03-components-scale-200.trace.txt) | root 3231×1631; body 3167×1567; header 3036×216; grid 3100×2761; `layout=pass` | [capture](stage-03-components-scale-200.png) |

Every trace also reports three generated select parts (`selectarrow`, `selectvalue`, `selectbox`) and four generated vertical-scrollbar parts (`slidertrack`, `sliderbar`, `sliderarrowdec`, `sliderarrowinc`). These are renderer/layout observations only. The four captures are separate production launches of the shared static fixture; they do not prove hover, keyboard, close, drag, resize, gameplay wiring, or assistive-technology behavior.

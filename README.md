# Ingnomia #

Just looking for the game itself?

Prebuilt binaries are available in the [release section](https://github.com/rschurade/Ingnomia/releases) or on
[Steam](https://store.steampowered.com/app/709240/Ingnomia/).

The active community for this game can be found on our [Discord server](https://discord.gg/y5GygwY).

## What is this? ##

Ingnomia started out as an independent remake of the older [Gnomoria](https://store.steampowered.com/app/224500/Gnomoria/) colony simulator, from which it was permitted to borrow some of the assets.
While the graphics look similar, all of the engine was rewritten from scratch. Compared to the reference, Ingnomias engine scales significantly better with large colonies.

In terms of features, balancing, user interface design etc. Ingnomia has since given its own spin to many of the core game elements.

While already in a playable state, it's still under heavy development and truly, by all means, "Early Access".
Not all game components have been implemented yet and some bugs are to be expected.

Ingnomia is a pure hobby project, and true free-to-play. With "free" spelled as in "free beer".

## Current fork development ##

This fork is based on the original Ingnomia project and currently focuses on a modernized, testable game interface while preserving the existing simulation and renderer seams.

The current development branch includes:

* A Qt 6/OpenGL application shell with RmlUi screens, overlays, draggable popup windows, and a unified game HUD.
* Reworked management screens for population, inventory, military, stockpiles, workshops, agriculture, missions, designations, and jobs.
* A shared UI foundation for navigation, actions, localization, accessibility checks, tutorials, and controller-level tests.
* Water rendering and simulation work, including dedicated water shaders, seeded world generation support, and flow-focused tests.
* A local Ingnomia MCP server and smoke-test tools under `tools/mcp/` for repeatable UI and runtime inspection.

This work is still in active development. The migration is being validated incrementally, so some screens and simulation systems may remain experimental between builds.

## How do I get set up for development? ##

The following steps describe how to compile the code locally and get the game running on Windows and Linux.

Note! Building on Mac is currently not possible. Certain features in the renderer require OpenGL4.3.

### Dependencies ###

#### Windows specific ####
* Microsoft Visual Studio 2022, the community edition is free
* Qt 6.9 desktop (MSVC 2022 on Windows)
#### All Platforms ####
* OpenGL 4.3 - Mac is not a supported compilation platform since it has deprecated OpenGL
* Qt 6.9 or newer
* RmlUi (resolved by the repository CMake dependency manifest; no external SDK or license is required)
* [Steam SDK](https://partner.steamgames.com/doc/sdk)
* OpenAL 1.25.1
* CMake 3.16 or newer

### Build ###

```bash
cp -r "<EXISTING_INGOMIA_INSTALLATION>/content/tilesheet" content/

cmake -S . -B "<BUILD_DIR>" \
-DQt6_DIR="<QTINSTALLDIR>/<ARCH>/lib/cmake/Qt6" \
-DSTEAM_SDK_ROOT="<STEAMSDKDIR>/sdk" \
-DOPENAL_ROOT="<OPENALSDKDIR>"
```

If no errors have occured, proceed by building the project with the chosen build system or open the generated project in an IDE of your choice.

```bash
cmake --build "<BUILD_DIR>"
```

### Building the documentation ###

Documentation building scripts are in the `docs/` directory. To rebuild documentation you will need:

* Python 3.9
* Pipenv
* Tilesheets from an existing installation (in the `content/tilesheet` directory, as when building the game)

From the `docs/` directory, run:

```bash
pipenv install
pipenv run ./generate.py
```

Output is generated in `docs/html`. You can specify a different output by passing `--output path/to/dir` to the generation script.

Generation will fail when the output directory exists. Use the `--overwrite` option in that case, but be warned that it will wipe the output directory completely.

### Forks on Github ###

The Windows CI build uses the public Qt, Steamworks, OpenAL, tile, and audio dependency archives listed in the workflow. No proprietary UI SDK or license secrets are required for forks.

Ingnomia comes with automatic builds via GitHub Actions, triggered on push to repository.
As long as you have forked Ingnomia as a **public** repository, these are expected to be free-of-charge for your GitHub account.

## Contribution guidelines ##

Help with Ignomia is always welcome.

### Making suggestions ###

We love to hear about your ideas on this game! Please head straight to our [Discord](https://discord.gg/DCSmxVD) server and discuss them in the #suggestions channel. You might see them realized at some point.

Please don't open tickets for suggestions on your own. That is reserved for already planed features.

### Reporting bugs ###

Even if you can't contribute in code, testing and reporting bugs is just as important to us. If you find something which doesn't behave right, or even crashes, feel free to open a ticket in the bugtracker.

Please include only one bug per ticket, with a precise step-by-step instruction how to trigger, and search for open tickets on the same subject first.

If you prefer, you may also try to reproduce bugs reported by other users. The more precise informations on a bug are, the higher the chance it can be fixed.

### Code contributions ###

If you know C++ or RmlUi/HTML/CSS you can help right away! Feel free to check for open bugs, and hop over to our [Discord](https://discord.gg/DCSmxVD) channel to get you sorted in.

Please provide your contributions in the form of a pull request, rebased onto the current head of development.

#### License ####

The contents of this repository are licensed under [GNU AFFERO GENERAL PUBLIC LICENSE Version 3](LICENSE). All contributions must adhere to the terms and conditions of this license. By submitting a pull request, you attest that you own the necessary rights on the code and you will abide to the AGPL3 license.

### Who do I talk to? ###

Hop over to our [Discord](https://discord.gg/DCSmxVD) server or open a ticket please.

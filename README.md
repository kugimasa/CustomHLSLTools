# Custom HLSL Tools

English | [日本語](README_ja.md)

An editor-only plugin for editing the HLSL used by Material **Custom nodes** in an external editor (VS Code / Rider / Visual Studio, etc.).

![Demo](docs/demo.gif)

## Features

- **Open Include Files in External Editor** button
  Added to the Custom node's Details panel. Opens the `.ush` / `.usf` files listed in `Include File Paths` in your external editor.
- **Create shader file** (Custom node's Details panel)
  Enter a name such as `MyFunc` (sub folders like `Sub/MyFunc` are allowed) and click **Create**. `<Project>/Shaders/MyFunc.ush` is generated with a function built from the node's inputs and output type, and the node's `Include File Paths`, `Code` and `Description` are set up to call it. Existing files are never overwritten, and `<Project>/Shaders` is created and mapped as `/Project` without restarting the editor.
  Until a shader file is attached, only **Create** is shown. Afterwards the **Shader File** field lets you rename the file: the file, the function inside it, `Include File Paths`, `Code` and `Description` are updated together.
- **Use an existing shader file**
  The **Existing Shader File** drop down lists the `.ush` files in `<Project>/Shaders` (shown only when there are any). Picking one sets up the node from its function signature; on a node that already has a file, it replaces the current `Include File Paths` entry.
- **Missing file handling**
  If a shader file is deleted outside the editor, the Details panel shows a warning with a **Recreate** button, and a notification offers to recreate all missing files. Materials that include a missing file are skipped by auto-recompile.
- **Set Up Node from Include File** button
  Reads the function signature of the first file in `Include File Paths` and sets `Inputs`, `Output Type` and `Code` from it. Inputs that keep their name stay connected. Supported return types: `float` / `float2` / `float3` / `float4` / `FMaterialAttributes` (`out` / `inout` parameters are not supported).
- **Auto-recompile on save**
  Watches `<Project>/Shaders` and the `Shaders` folders of enabled non-Engine plugins. When a `.ush` / `.usf` file is saved, loaded materials that include files from that folder are recompiled.
- **Plugin icon** in the Details panel marks the rows added by this plugin.
- **Settings** (`Editor Preferences > Plugins > Custom HLSL Tools`)
  How to launch the external editor (Source Code Accessor / OS file association / custom command), auto-recompile ON/OFF, and the delay before recompiling.
  If you choose `Source Code Accessor`, the editor selected in `Editor Preferences > General > Source Code` is used.

## Usage

1. Create `<Project>/Shaders/MyFunc.ush` and define a function.
   ```hlsl
    float3 MyFunc(float3 A, float B)
    {
        return A * B;
    }
   ```
2. Restart the editor. The `/Project` virtual path is registered only if `<Project>/Shaders` exists at startup.
3. Add `/Project/MyFunc.ush` to the Custom node's `Include File Paths`, and write `return MyFunc(A, B);` in `Code`.
4. Click **Open Include Files in External Editor** in Details, edit the file, and save.

## Installation

Place this repository at `<Project>/Plugins/CustomHLSLTools`.

## How it works / Limitations

- The content hash of include files is part of the material's shader map ID, so recompiling after `FlushShaderFileCache()` picks up the changes.
- Virtual paths are resolved with `GetShaderSourceFilePath()`, so only the `.ush` / `.usf` extensions are valid.
- A material is recompiled if it includes a file under a watched folder where a change occurred. Changes to files that are further included from a `.ush` are also detected, but a change to a different file in the same folder may also trigger a recompile.
- Material instances are updated when their parent material is recompiled.

## Supported versions

UE 5.8

## License

The code of this plugin is provided under the [MIT License](LICENSE).

Unreal Engine itself is governed by Epic Games' [Unreal Engine EULA](https://www.unrealengine.com/eula).

## Support

Please report bugs and requests via [GitHub Issues](https://github.com/kugimasa/CustomHLSLTools/issues).

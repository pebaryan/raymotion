# Programming language badge prefabs

The built-in `language-<id>` prefabs use the same props as `social-<id>`: `size`, `style` (`badge`, `glyph`, `mono`, `outline`), `shape`, `color`, `handle`, `ink`, `in`, `delay`, and `fps`.

```cpp
prefab::use(root, 0, 90, "language-python", Json::parse(R"({"size": 160, "in": "pop"})"));
```

Available IDs: `python`, `javascript`, `typescript`, `c`, `cplusplus`, `csharp`, `java`, `go`, `rust`, `ruby`, `php`, `swift`, `kotlin`, `dart`, `r`, `lua`, `haskell`, `julia`. `icons::languages()` returns them in catalogue order, and `icons::glyph(id)` returns the vector path.

The badge treatment uses a single white mark on each language's catalogue colour. JavaScript and C use dark marks for contrast. These monochrome badges simplify logos that normally have multiple colours.

Paths and colours come from [Simple Icons 16.36.0](https://github.com/simple-icons/simple-icons/tree/16.36.0). C# uses its [archived Simple Icons 12.4.0 path](https://github.com/simple-icons/simple-icons/blob/12.4.0/icons/csharp.svg), and Java uses the [Devicon Java plain path](https://github.com/devicons/devicon/blob/master/icons/java/java-plain.svg), scaled from its 128-unit view box (Devicon is MIT-licensed, © 2015 konpa; the notice is kept in the source file). The paths are embedded in `src/engine/language_brands.inc`; rendering has no network dependency.

# Marketplace badge prefabs

Six marketplace marks are registered as built-in `social-*` prefabs:

| Store | Prefab | Badge colour | Mark source |
| --- | --- | --- | --- |
| Shopee | `social-shopee` | `#EE4D2D` | [Simple Icons SVG](https://github.com/simple-icons/simple-icons/blob/develop/icons/shopee.svg) |
| Tokopedia | `social-tokopedia` | `#03AC0E` | Compact owl/bag adaptation; [reference](https://www.svgviewer.dev/s/34686/tokopedia) |
| Amazon | `social-amazon` | `#FF9900` | [Simple Icons 12.4.0 SVG](https://github.com/simple-icons/simple-icons/blob/12.4.0/icons/amazon.svg) |
| eBay | `social-ebay` | `#E53238` | [Simple Icons SVG](https://github.com/simple-icons/simple-icons/blob/develop/icons/ebay.svg) |
| Vinted | `social-vinted` | `#007782` | [Simple Icons SVG](https://github.com/simple-icons/simple-icons/blob/develop/icons/vinted.svg) |

Use them like the existing social and gaming badges:

```cpp
auto* badge = prefab::use(root, 0, 90, "social-vinted",
    Json::parse(R"({"size": 160, "style": "badge", "in": "pop"})"));
badge->props.pos = { 960, 540 };
```

`style` accepts `badge`, `glyph`, `mono`, or `outline`; `shape` accepts `rounded`, `circle`, or `square`. Each is also usable through `icons::draw`, `icons::glyph`, `icons::title`, and `icons::brandColor`. The `social-row` prefab accepts these slugs in its `brands` list.

The Tokopedia mark is an illustrative monochrome adaptation. The brand names and marks remain the property of their owners.

[View the 18-badge rendered sheet](images/prefabs/social-badge-sheet.png). Rebuild it with:

```powershell
cmake --build build --target raymotion --config Release
.\build\raymotion.exe still social-badge-sheet --at 0 --png out\social-badge-sheet.png
```

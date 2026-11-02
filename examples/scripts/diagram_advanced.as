void configure(Composition &inout c) {
    c.name = "diagram-advanced";
    c.w = 960; c.h = 540; c.fps = 30; c.duration = 90;
    c.background = 0x11151e;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    draw::text("DIAGRAM ACTIONS", 54, 44, 28, 0xf0f3f7);
    draw::text("BUILD  /  TRACE  /  MORPH", 54, 78, 14, 0x70d7ff);
    draw::roundRect(54, 112, 852, 1, 0, 0x293341);

    DiagramGraph@ data = diagram::newGraph();
    data.setDirection(DIRECTION_LR); data.setLayout(LAYOUT_LAYERED);
    int source = data.addNode("source", "Source");
    int parse = data.addNode("parse", "Parse");
    int finish = data.addNode("finish", "Render");
    data.setNodeShape(source, SHAPE_RECT);
    data.setNodeShape(parse, SHAPE_RECT);
    data.setNodeShape(finish, SHAPE_RECT);
    data.addEdge("source", "parse");
    data.addEdge("parse", "finish");
    string graph = data.toJson();
    string options = "{\"theme\":\"dark\",\"maxZoom\":1.45,\"margin\":30,\"layout\":{\"kind\":\"layered\",\"direction\":\"LR\",\"nodeSpacing\":52,\"rankSpacing\":110},\"themeStyle\":{\"highlight\":\"#ffbf69\",\"edge\":\"#6c829c\",\"fill\":\"#263648\",\"stroke\":\"#6386a8\",\"colorKinds\":false,\"fontCandidates\":[\"SpaceGrotesk-Variable.ttf\"],\"boldFontCandidates\":[\"ArchivoBlack-Regular.ttf\"]},\"actions\":[{\"kind\":\"build\",\"at\":8,\"stagger\":4,\"duration\":12},{\"kind\":\"highlight\",\"at\":22,\"duration\":8,\"ids\":[\"parse\"],\"color\":\"#ffbf69\"},{\"kind\":\"hide\",\"at\":30,\"duration\":6,\"ids\":[\"parse\"]},{\"kind\":\"reveal\",\"at\":39,\"duration\":8,\"ids\":[\"parse\"]},{\"kind\":\"unhighlight\",\"at\":49,\"duration\":1,\"ids\":[\"parse\"]},{\"kind\":\"flow\",\"at\":51,\"duration\":18,\"edges\":[\"source->parse\",\"parse->finish\"],\"count\":3,\"color\":\"#70d7ff\"},{\"kind\":\"trace\",\"at\":54,\"duration\":18,\"path\":[\"source\",\"parse\",\"finish\"],\"color\":\"#ffbf69\"},{\"kind\":\"morph\",\"at\":65,\"duration\":16,\"source\":\"graph LR\\n  source[Source] --> parse[Parse] --> export[Export]\",\"format\":\"mermaid\"}]}";
    diagram::drawAdvanced(graph, "json", 72, 150, 816, 300, frame, options);
    if (frame < 16) draw::text("1 / BUILD", 54, 484, 14, 0x9aa8b7);
    else if (frame < 48) draw::text("2 / INSPECT THE NODE", 54, 484, 14, 0x9aa8b7);
    else if (frame < 56) draw::text("3 / FOLLOW THE ROUTE", 54, 484, 14, 0x9aa8b7);
    else draw::text("4 / MORPH THE GRAPH", 54, 484, 14, 0x9aa8b7);
}

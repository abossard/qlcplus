# QLC+ MCP Server — Architecture & Visualization

> Auto-generated analysis of the MCP server in this project.
> Source files referenced: [`mcp/`](../mcp/)

---

## 1. Layered Overview

Shows how an AI agent request flows from HTTP through MCP protocol, tool dispatch,
the VCBridge abstraction, and into the QLC+ engine.

```mermaid
graph TB
    subgraph "External"
        Agent["AI Agent<br/>(Claude, GPT, etc.)"]
    end

    subgraph "Transport Layer"
        HTTP["StreamableHttpServerWrapper<br/>127.0.0.1:9696/mcp"]
    end

    subgraph "MCP Protocol Layer"
        Handler["fastmcpp::mcp::Handler<br/>(JSON-RPC 2.0)"]
        Server["fastmcpp::server::Server<br/>name='qlcplus' v5.0.0"]
    end

    subgraph "Manager Layer"
        TM["ToolManager<br/>(30+ tools)"]
        PM["PromptManager<br/>(design_dj_show)"]
        RM["ResourceManager<br/>(unused)"]
    end

    subgraph "Tool Registration Layer"
        QT["registerQueryTools<br/>query_tools.cpp"]
        FT["registerFunctionTools<br/>function_tools.cpp"]
        VCC["registerVCCreateTools<br/>vc_create_tools.cpp"]
        VCU["registerVCUpdateTools<br/>vc_update_tools.cpp"]
        VCI["registerVCInputTools<br/>vc_input_tools.cpp"]
        VCL["registerVCLayoutTools<br/>vc_layout_tools.cpp"]
        IO["registerIOTools<br/>io_tools.cpp"]
        CH["registerChannelTools<br/>channel_tools.cpp"]
        PR["registerPrompts<br/>prompts.cpp"]
    end

    subgraph "Abstraction Layer"
        VCB["VCBridge<br/>(abstract interface)"]
        VCB5["VCBridgeV5<br/>(QML implementation)"]
    end

    subgraph "QLC+ Engine"
        Doc["Doc<br/>(document model)"]
        IOM["InputOutputMap"]
        FC["FixtureDefCache"]
        FM["FunctionManager"]
        VC["VirtualConsole"]
    end

    subgraph "QLC+ Data Model"
        Fix["Fixture"]
        Scene["Scene"]
        Chaser["Chaser"]
        EFX["EFX"]
        Coll["Collection"]
        Seq["Sequence"]
        RGBm["RGBMatrix"]
        Uni["Universe"]
    end

    Agent -->|"HTTP POST"| HTTP
    HTTP --> Handler
    Handler --> Server
    Server --> TM
    Server --> PM
    Server --> RM

    TM --> QT & FT & VCC & VCU & VCI & VCL & IO & CH
    PM --> PR

    QT -->|"query_fixtures<br/>patch_fixtures<br/>query_functions<br/>query_available_fixtures<br/>vc_query_pages<br/>vc_query_widgets"| Doc
    QT -->|"vc_query_*"| VCB

    FT -->|"create_scenes<br/>create_chasers<br/>create_sequences<br/>create_efxs<br/>create_collections<br/>create_rgb_matrices<br/>create_scripts<br/>delete_functions"| Doc

    VCC & VCU & VCI & VCL -->|"all vc_* tools"| VCB
    VCB -.->|"implements"| VCB5
    VCB5 --> VC

    IO -->|"configure_universes<br/>configure_osc<br/>query_midi_devices"| IOM
    CH -->|"query_fixture_channels<br/>configure_channels"| Doc

    Doc --> Fix & Scene & Chaser & EFX & Coll & Seq & RGBm
    IOM --> Uni

    style Agent fill:#e1f5fe
    style HTTP fill:#fff3e0
    style Handler fill:#fff3e0
    style VCB fill:#f3e5f5
    style VCB5 fill:#f3e5f5
    style Doc fill:#e8f5e9
```

### Initialization Sequence

```mermaid
sequenceDiagram
    participant Main as qmlui/main.cpp
    participant Init as mcpinit.cpp
    participant Bridge as VCBridgeV5
    participant Server as McpServer
    participant HTTP as StreamableHttpServer
    participant TM as ToolManager

    Main->>Init: mcpInit(doc, vc, funcMgr, parser)
    Init->>Init: Check --no-mcp flag
    
    alt vc != nullptr
        Init->>Bridge: new VCBridgeV5(doc, vc)
    else vc == nullptr
        Note over Init: vcBridge stays nullptr<br/>VC tools won't register
    end
    
    Init->>Server: new McpServer(doc, vcBridge, funcMgr)
    
    Server->>TM: registerQueryTools(tm, doc, vcBridge)
    Note over TM: query_fixtures, patch_fixtures<br/>always registered
    Note over TM: vc_query_pages, vc_query_widgets<br/>only if vcBridge != null

    Server->>TM: registerFunctionTools(tm, doc, funcMgr)
    Server->>TM: registerVCCreateTools(tm, doc, vcBridge)
    Note over TM: if (!vcBridge) return; // guard
    Server->>TM: registerVCUpdateTools(tm, doc, vcBridge)
    Server->>TM: registerVCInputTools(tm, doc, vcBridge)
    Server->>TM: registerVCLayoutTools(tm, doc, vcBridge)
    Server->>TM: registerIOTools(tm, doc)
    Server->>TM: registerChannelTools(tm, doc)
    Server->>TM: registerPrompts(pm, doc)

    Init->>Server: startHttp(port)
    Server->>HTTP: new StreamableHttpServerWrapper(handler, "127.0.0.1", port, "/mcp")
    HTTP->>HTTP: start()
    Note over HTTP: Listening on http://127.0.0.1:9696/mcp
```

**Ref:** [mcpinit.cpp](mcpinit.cpp) | [mcpserver.cpp](mcpserver.cpp) | [tool_registry.h](tools/tool_registry.h)

---

## 2. Tool Inventory — Complete Map

```mermaid
mindmap
  root((QLC+ MCP<br/>30+ Tools))
    **Query Tools**<br/>query_tools.cpp
      query_fixtures 🔍
      query_available_fixtures 🔍
      patch_fixtures ♻️
      query_functions 🔍
      vc_query_pages 🔍
      vc_query_widgets 🔍
    **Function Tools**<br/>function_tools.cpp
      create_scenes ♻️
      create_chasers ♻️
      create_sequences ♻️
      create_efxs ♻️
      create_collections ♻️
      create_rgb_matrices ♻️
      create_scripts ♻️
      delete_functions 💥
    **VC Create**<br/>vc_create_tools.cpp
      vc_create_pages ♻️
      vc_create_widgets ♻️
    **VC Update**<br/>vc_update_tools.cpp
      vc_update_widgets ♻️
    **VC Input**<br/>vc_input_tools.cpp
      vc_map_inputs ♻️
      vc_configure_feedback ♻️
      vc_set_key_sequences ♻️
    **VC Layout**<br/>vc_layout_tools.cpp
      vc_reparent_widgets ♻️
      vc_delete_widgets 💥
      vc_detect_overlaps 🔍
      vc_reflow_frame ♻️
    **I/O Tools**<br/>io_tools.cpp
      configure_universes 🌐
      configure_plugin_params 🌐
      query_midi_devices 🔍
      query_input_profiles 🔍
      set_input_profile ♻️
      query_feedback_profile 🔍
      configure_osc 🌐
      query_osc_status 🔍
      configure_beat_source 🌐
    **Channel Tools**<br/>channel_tools.cpp
      query_fixture_channels 🔍
      configure_channels ♻️
      query_channel_modifiers 🔍
      set_channel_modifiers ♻️
      convert_degrees_to_dmx 🔍
```

**Legend:** 🔍 = readOnly | ♻️ = idempotent | 💥 = destructive | 🌐 = openWorld

---

## 3. Query Tools — Detail

```mermaid
classDiagram
    class query_fixtures {
        <<readOnly>>
        +input: none
        +output: Fixture[]
        --
        Ref: query_tools.cpp
        Calls: Doc::fixtures()
        Converts via: fixtureToJson()
    }

    class query_available_fixtures {
        <<readOnly>>
        +input: items[manufacturer?, model?]
        +output: FixtureDef[]
        --
        Ref: query_tools.cpp
        Calls: Doc::fixtureDefCache()
        Substring match on manufacturer/model
    }

    class patch_fixtures {
        <<idempotent>>
        +input: items[manufacturer, model, mode, name, universe, address, quantity?]
        +output: PatchResult[]
        --
        Ref: query_tools.cpp
        Calls: Doc::addFixture()
        Upsert via: findFixture(name, universe, address)
    }

    class query_functions {
        <<readOnly>>
        +input: none
        +output: Function[]
        --
        Ref: query_tools.cpp
        Calls: Doc::functions()
        Converts via: functionToJson()
    }

    class vc_query_pages {
        <<readOnly>>
        +input: none
        +output: Page[widgets: WidgetDetails[]]
        --
        Ref: query_tools.cpp
        Calls: VCBridge::pages()
        Enriched via: VCBridge::getWidgetDetails()
        GUARD: only registered if vcBridge != null
    }

    class vc_query_widgets {
        <<readOnly>>
        +input: widgetIDs: int[]
        +output: WidgetDetails[]
        --
        Ref: query_tools.cpp
        Calls: VCBridge::getWidgetDetails()
        GUARD: only registered if vcBridge != null
    }

    class Fixture {
        +id: int
        +name: string
        +universe: int
        +address: int
        +channels: int
        +heads: int
        +manufacturer: string
        +model: string
        +mode: string
        +capabilities: string[]
        +physical: Physical
    }

    class Physical {
        +weight: float
        +width/height/depth: int
        +bulbType: string
        +focusPanMax: int
        +focusTiltMax: int
    }

    class FunctionSummary {
        +id: int
        +name: string
        +type: string
        +duration: int
        +fixtureCount: int
        +stepCount: int
        +runOrder: string
        +tempoType: string
    }

    query_fixtures --> Fixture
    query_available_fixtures --> Fixture
    patch_fixtures --> Fixture
    query_functions --> FunctionSummary
    Fixture --> Physical
```

**Ref:** [query_tools.cpp](tools/query_tools.cpp) | [conversions.h](tools/conversions.h)

---

## 4. Function Tools — Detail

```mermaid
classDiagram
    class create_scenes {
        <<idempotent>>
        +name: string ⚠️required
        +fixtureIDs: int[]
        +fixtureNames: string[] (glob)
        +fadeIn/fadeOut: int (ms)
        +channelValues: ChannelValue[]
        +positions: Position[]
        --
        Ref: function_tools.cpp
        Upsert: findFunction(name, SceneType)
        On upsert: scene.clear() then re-apply
        Engine: Scene::setValue(SceneValue)
        Engine: Fixture::positionToValues()
    }

    class create_chasers {
        <<idempotent>>
        +name: string
        +steps: ChaserStep[]
        +tempoType: "time"|"beats"
        +runOrder: "loop"|"singleshot"|"pingpong"|"random"
        +direction: "forward"|"backward"
        --
        Ref: function_tools.cpp
        Upsert: findFunction(name, ChaserType)
        Engine: Chaser::addStep(ChaserStep)
        Beat encoding: 1000=1beat
    }

    class create_sequences {
        <<idempotent>>
        +name: string
        +boundSceneID / boundSceneName: int|string
        +fadeIn/fadeOut/holdTime: int
        +runOrder/direction: string
        --
        Ref: function_tools.cpp
        Engine: Sequence::setBoundSceneID()
        ⚠️ Sequence is subclass of Chaser
    }

    class create_efxs {
        <<idempotent>>
        +name: string
        +fixtureIDs / fixtureNames
        +algorithm: enum
        +width/height: int (0-255)
        +xOffset/yOffset: int
        +rotation: int (0-360)
        +speed/duration: int
        +propagationMode: "parallel"|"serial"|"asymmetric"
        --
        Algorithms: Circle, Eight, Line, Diamond,
        Square, SquareChoppy, Leaf, Lissajous,
        Triangle, SquareTrue
        Engine: EFX::addFixture(EFXFixture)
    }

    class create_collections {
        <<idempotent>>
        +name: string
        +functionIDs: int[]
        +functionNames: string[]
        --
        Ref: function_tools.cpp
        Engine: Collection::addFunction()
        Resolves names via resolveFunctionByName()
    }

    class create_rgb_matrices {
        <<idempotent>>
        +name: string
        +fixtureGroupID / fixtureGroupName
        +algorithm: string
        +startColor/endColor: hex color
        +fadeIn/fadeOut: int
        +runOrder/direction: string
        --
        Ref: function_tools.cpp
        Engine: RGBMatrix::setAlgorithm()
        Requires a FixtureGroup
    }

    class create_scripts {
        <<idempotent>>
        +name: string
        +commands: string[]
        --
        Ref: function_tools.cpp
        Engine: Script::setData()
    }

    class delete_functions {
        <<destructive>>
        +functionIDs: int[]
        +functionNames: string[]
        --
        Ref: function_tools.cpp
        Engine: Doc::deleteFunction()
    }

    class ChannelValue {
        +fixtureID: int
        +channel: int
        +value: int (0-255)
    }

    class Position {
        +fixtureID: int
        +panDegrees: float
        +tiltDegrees: float
        +zoomDegrees: float
    }

    class ChaserStep {
        +functionID / functionName
        +fadeIn/hold/fadeOut: int
        +note: string
    }

    create_scenes --> ChannelValue
    create_scenes --> Position
    create_chasers --> ChaserStep
```

**Ref:** [function_tools.cpp](tools/function_tools.cpp) | [idempotency.h](tools/idempotency.h)

---

## 5. Virtual Console Tools — Widget Type Map

```mermaid
classDiagram
    class vc_create_widgets {
        <<idempotent>>
        +type: WidgetType (discriminator)
        +parentID: int
        +caption: string
        +upsert: bool (default true)
        +x/y/width/height: int
        +bgColor/fgColor: hex
        +functionID / functionName
        -- type-specific properties --
    }

    class WidgetType {
        <<enumeration>>
        button = 1
        slider = 2
        xypad = 3
        frame = 4
        soloframe = 5
        speedDial = 6
        cuelist = 7
        label = 8
        audioTrigger = 9
        matrix = 10
        clock = 11
    }

    class ButtonProps {
        +action: toggle|flash|blackout|stopall
        +iconPath: string
        +startupIntensity: 0.0-1.0
        +flashOverride/flashForceLTP: bool
        +stopAllFadeTime: int (ms)
    }

    class SliderProps {
        +mode: level|playback|submaster|grandmaster
        +widgetStyle: slider|knob
        +channels: [fixtureID, channel][]
        +clickAndGoType: none|colors|preset|rgb|cmy
        +valueDisplayStyle: dmx|percentage
        +rangeLowLimit/rangeHighLimit: 0-255
        +monitorEnabled: bool
        +gmValueMode: limit|reduce
        +gmChannelMode: intensity|allchannels
    }

    class FrameProps {
        +pageIndex: int (create only)
        +solo: bool (create only)
        +multipageMode: bool
        +totalPages: int
        +pagesLoop: bool
        +pageLabels: string[]
        +headerVisible/enableButtonVisible: bool
        +soloframeMixing: bool ★soloframe only
        +excludeMonitoredFunctions: bool ★soloframe only
    }

    class XYPadProps {
        +fixtureIDs: int[] (simple)
        +fixtures: XYPadFixtureConfig[] (advanced)
        +displayMode: degrees|percentage|dmx
        +invertedAppearance: bool
    }

    class CueListProps {
        +chaserID / chaserName
        +nextPrevBehavior: defaultRunFirst|runNext|select|nothing
        +playbackLayout: playPauseStop|playStopPause
        +sideFaderMode: none|crossfade|steps
    }

    class SpeedDialProps {
        +functions: SpeedDialFunctionInfo[]
        +absoluteValueMin/Max: int
        +visibilityMask: int
        +resetFactorOnDialChange: bool
    }

    class AudioTriggerProps {
        +captureEnabled: bool
        +volumeLevel: int
        +barsNumber: int
    }

    class MatrixProps {
        +instantApply: bool
        +visibilityMask: int
    }

    class ClockProps {
        +clockType: clock|stopwatch|countdown
        +countdownH/M/S: int
        +schedules: ClockScheduleInfo[]
    }

    vc_create_widgets --> WidgetType
    WidgetType --> ButtonProps
    WidgetType --> SliderProps
    WidgetType --> FrameProps
    WidgetType --> XYPadProps
    WidgetType --> CueListProps
    WidgetType --> SpeedDialProps
    WidgetType --> AudioTriggerProps
    WidgetType --> MatrixProps
    WidgetType --> ClockProps
```

**Ref:** [vc_create_tools.cpp](tools/vc_create_tools.cpp) | [vc_tools_common.h](tools/vc_tools_common.h)

---

## 6. VC Input Mapping & Feedback

```mermaid
flowchart LR
    subgraph "Physical Controllers"
        MIDI["MIDI Controller<br/>(Launchpad, etc.)"]
        OSC["OSC Device"]
    end

    subgraph "vc_map_inputs"
        MI["Map input source<br/>by named source"]
    end

    subgraph "vc_configure_feedback"
        FB["Set LED feedback<br/>idle/active/monitor values<br/>+ MIDI channels"]
    end

    subgraph "vc_set_key_sequences"
        KS["Set keyboard shortcut<br/>per source name"]
    end

    subgraph "Widget Source Names"
        direction TB
        BTN["Button<br/>• default"]
        SLD["Slider<br/>• default<br/>• overrideReset<br/>• flashButton"]
        CUE["CueList<br/>• next, previous<br/>• playback, stop<br/>• sideFader"]
        XYP["XYPad<br/>• pan, panFine<br/>• tilt, tiltFine<br/>• width, height<br/>• preset0..N"]
        SPD["SpeedDial<br/>• absolute, tap<br/>• mult, div, apply<br/>• 1_16x..16x<br/>• preset0..N"]
        FRM["Frame/SoloFrame<br/>• nextPage, previousPage<br/>• enable, collapse<br/>• page0..N"]
        CLK["Clock<br/>• play, reset"]
        AUD["AudioTriggers<br/>• default<br/>• volumeControl"]
        MAT["Matrix<br/>• default"]
    end

    MIDI --> MI
    OSC --> MI
    MI --> BTN & SLD & CUE & XYP & SPD & FRM & CLK & AUD & MAT
    FB --> BTN & SLD & CUE & XYP & SPD & FRM & CLK & AUD & MAT
    KS --> BTN & SLD & CUE & XYP & SPD & FRM & CLK & AUD & MAT

    style MI fill:#e8f5e9
    style FB fill:#fff3e0
    style KS fill:#e1f5fe
```

**Ref:** [vc_input_tools.cpp](tools/vc_input_tools.cpp)

---

## 7. I/O & Channel Tools

```mermaid
flowchart TB
    subgraph "Universe Configuration"
        CU["configure_universes<br/>Set input/output plugin per universe<br/>ArtNet, E1.31, OSC, MIDI"]
        CP["configure_plugin_params<br/>Set plugin-specific params<br/>(initmessage, midichannel)"]
        CO["configure_osc<br/>One-call OSC setup<br/>(ports, IPs, feedback)"]
        CB["configure_beat_source<br/>Set internal/MIDI beat source"]
    end

    subgraph "Profile Management"
        QIP["query_input_profiles 🔍"]
        SIP["set_input_profile ♻️"]
        QFP["query_feedback_profile 🔍<br/>Color table + MIDI channel table"]
    end

    subgraph "Device Discovery"
        QMD["query_midi_devices 🔍<br/>List all plugins + ports"]
        QOS["query_osc_status 🔍"]
    end

    subgraph "Channel Tools"
        QFC["query_fixture_channels 🔍<br/>Per-channel capabilities,<br/>DMX ranges, presets"]
        CC["configure_channels ♻️<br/>Set precedence: auto/htp/ltp<br/>Set canFade per channel"]
        QCM["query_channel_modifiers 🔍<br/>List modifier templates"]
        SCM["set_channel_modifiers ♻️<br/>Apply modifier to channel"]
        CDM["convert_degrees_to_dmx 🔍<br/>Pan/tilt degree→DMX conversion"]
    end

    subgraph "QLC+ Engine"
        IOM["InputOutputMap"]
        IOC["IOPluginCache"]
        UNI["Universe"]
        FIX["Fixture"]
    end

    CU --> IOM
    CP --> IOC
    CO --> IOM
    CO --> IOC
    CB --> IOM
    QMD --> IOC
    QOS --> IOC
    QIP --> IOM
    SIP --> IOM
    QFP --> IOM
    QFC --> FIX
    CC --> FIX
    QCM --> FIX
    SCM --> FIX
    CDM --> FIX
    IOM --> UNI
```

**Ref:** [io_tools.cpp](tools/io_tools.cpp) | [channel_tools.cpp](tools/channel_tools.cpp) | [conversions.h](tools/conversions.h)

---

## 8. VCBridge Abstraction — Interface vs Implementation

```mermaid
classDiagram
    class VCBridge {
        <<abstract>>
        +addPage(name) int
        +pages() PageInfo[]
        +pagesCount() int
        +addFrame(pageIndex, geometry, caption, solo) int
        +addFrameInFrame(parentID, geometry, caption, solo) int
        +addButton(parentID, geometry, functionID, caption, action) int
        +addSlider(parentID, geometry, mode, caption, functionID, channels) int
        +addXYPad(parentID, geometry, fixtureIDs) int
        +addXYPadEx(parentID, geometry, fixtures, displayMode, inverted) int
        +addCueList(parentID, geometry, chaserID, caption) int
        +addLabel(parentID, geometry, text) int
        +addSpeedDial(parentID, geometry, functionIDs) int
        +addAudioTriggers(parentID, geometry) int
        +addClock(parentID, geometry, clockType) int
        +addMatrix(parentID, geometry, functionID, caption) int
        +mapWidgetInput(widgetID, universe, channel) bool
        +mapWidgetInputByName(widgetID, sourceName, universe, channel) bool
        +setWidgetFeedback(...) bool
        +setWidgetFeedbackByName(...) bool
        +getWidgetDetails(widgetID) WidgetDetails
        +setWidgetCaption(widgetID, caption) bool
        +setWidgetColors(widgetID, bg, fg) bool
        +setWidgetFont(widgetID, font) bool
        +configureButton(widgetID, config) bool
        +configureSlider(widgetID, config) bool
        +configureFrame(widgetID, config) bool
        +configureCueList(widgetID, config) bool
        +configureClock(widgetID, config) bool
        +configureSpeedDial(widgetID, config) bool
        +configureMatrix(widgetID, config) bool
        +reparentWidget(widgetID, newParentID, geo) bool
        +removeWidget(widgetID) bool
        +findWidgetByCaption(parentID, type, caption) int
        +findPageByName(name) int
        +reflowPage(page, opts) LayoutPlan$
        +detectOverlaps(siblings) OverlapInfo[]$
    }

    class VCBridgeV5 {
        -m_doc: Doc*
        -m_vc: VirtualConsole*
        +implements ALL virtual methods
        --
        Maps to QML VirtualConsole API
        Uses qobject_cast for widget types
        Direct VCWidget manipulation
    }

    class WidgetDetails {
        +id: int
        +type: string
        +caption: string
        +geometry: QRect
        +functionID: quint32
        +action: string
        +sliderMode: string
        +channels: [fixtureID, channel][]
        +inputMappings: InputMapping[]
        +bgColor/fgColor: QColor
        +validSources: SourceDef[]
        +parentID: int
        -- Button extended --
        +iconPath, startupIntensity, flashOverride...
        -- Slider extended --
        +clickAndGoType, valueDisplayStyle...
        -- Frame extended --
        +multipageMode, totalPages, currentPage...
        -- CueList extended --
        +nextPrevBehavior, playbackLayout...
        -- SpeedDial extended --
        +speedDialFunctions, speedDialPresets...
        -- AudioTriggers extended --
        +audioBars, captureEnabled, volumeLevel...
        -- XYPad extended --
        +xyPadFixtures, xyPadPosition, displayMode...
        -- Clock/Matrix extended --
        +clockType, matrixAnimation...
    }

    class InputMapping {
        +universe: quint32
        +channel: quint32
        +sourceId: quint32
        +sourceName: string
        +feedback: FeedbackInfo
    }

    class FeedbackInfo {
        +idleValue: int
        +activeValue: int
        +monitorValue: int
        +idleMidiCh: int
        +activeMidiCh: int
        +monitorMidiCh: int
    }

    VCBridge <|-- VCBridgeV5
    VCBridge --> WidgetDetails
    WidgetDetails --> InputMapping
    InputMapping --> FeedbackInfo

    note for VCBridge "Ref: vcbridge.h\n~700 lines\nAll methods have default\nimplementations (return -1/false)\nso V4 bridge could be added later"
    note for VCBridgeV5 "Ref: vcbridgev5.h/cpp\n~2000 lines\nOnly implementation that exists"
```

**Ref:** [vcbridge.h](vcbridge.h) | [vcbridgev5.h](vcbridgev5.h) | [vcbridgev5.cpp](vcbridgev5.cpp)

---

## 9. Thread Safety Model

```mermaid
sequenceDiagram
    participant HTTP as HTTP Thread<br/>(fastmcpp)
    participant Guard as execOnMainThread<br/>tool_registry.h
    participant Main as Main/UI Thread<br/>(Qt Event Loop)
    participant Doc as Doc / VCBridge

    HTTP->>Guard: Tool handler invoked with JSON args
    
    alt Same thread (unlikely)
        Guard->>Guard: Call func() directly
    else Different thread (normal case)
        Guard->>Main: QMetaObject::invokeMethod<br/>Qt::BlockingQueuedConnection
        Main->>Doc: Access Doc/VCBridge safely
        Doc-->>Main: Result
        Main-->>Guard: JSON result string
    end
    
    Guard-->>HTTP: Return JSON response

    Note over Guard: try/catch wraps all lambdas<br/>Prevents crashes from<br/>malformed JSON
```

**Ref:** [tool_registry.h](tools/tool_registry.h) — `execOnMainThread()` template

---

## 10. Idempotency & Resolution Strategy

```mermaid
flowchart TB
    subgraph "Name Resolution"
        FN["Function by Name<br/>findFunction(doc, name, type)"]
        FX["Fixture by Name/Addr<br/>findFixture(doc, name, uni, addr)"]
        FG["FixtureGroup by Name<br/>findFixtureGroup(doc, name)"]
        FP["Fixture by Pattern<br/>resolveFixturesByName(doc, glob)"]
    end

    subgraph "Upsert Pattern"
        direction TB
        A["Receive create request"] --> B{"findFunction(name, type)"}
        B -->|"Found"| C["Reset existing object<br/>(e.g. scene.clear())"]
        B -->|"Not found"| D["Create new object"]
        C --> E["Apply new properties"]
        D --> E
        E --> F["doc.addFunction() if new"]
        F --> G["Return {id, status: created|updated}"]
    end

    subgraph "Widget Upsert"
        W1["vc_create_widgets with upsert=true"] --> W2{"findWidgetByCaption<br/>(parentID, type, caption)"}
        W2 -->|"Found"| W3["Return existing widget ID"]
        W2 -->|"Not found"| W4["Create new widget"]
    end

    FN --> B
    FX -.-> A
    FP --> FX

    style A fill:#e8f5e9
    style FN fill:#e1f5fe
    style FX fill:#e1f5fe
    style FG fill:#e1f5fe
    style FP fill:#e1f5fe
```

**Ref:** [idempotency.h](tools/idempotency.h)

---

## 11. Field Validation Pipeline

```mermaid
flowchart LR
    subgraph "Input JSON"
        REQ["{ type: 'button',<br/>  parentID: 1,<br/>  caption: 'Go',<br/>  badField: 123 }"]
    end

    subgraph "Stage 1: Field Names"
        V1["VCValidate::<br/>validateFieldsForType()"]
        V1a["Merge commonCreate +<br/>createFieldsForType(Button)"]
        V1b{"Unknown fields?"}
    end

    subgraph "Stage 2: Field Values"
        V2["VCValidate::<br/>validateFieldValues()"]
        V2a{"Valid enum?<br/>Valid range?<br/>Valid hex color?"}
    end

    subgraph "Stage 3: Tool-level"
        V3["validateFields()<br/>(from tool_registry.h)"]
    end

    REQ --> V1
    V1 --> V1a --> V1b
    V1b -->|"Yes"| ERR1["Error: field 'badField'<br/>not valid for 'button'"]
    V1b -->|"No"| V2
    V2 --> V2a
    V2a -->|"Invalid"| ERR2["Error: invalid value 'xyz'<br/>for field 'action'"]
    V2a -->|"Valid"| OK["Proceed to handler"]

    style ERR1 fill:#ffcdd2
    style ERR2 fill:#ffcdd2
    style OK fill:#c8e6c9
```

**Ref:** [vc_tools_common.h](tools/vc_tools_common.h) — `VCValidate` namespace | [tool_registry.h](tools/tool_registry.h) — `validateFields()`

---

## 12. Recommended Workflow

```mermaid
flowchart TB
    A["1. query_fixtures<br/>→ Get fixture IDs"] --> B["2. query_fixture_channels<br/>→ Discover channel indices"]
    B --> C["3. create_scenes<br/>→ Build mood/dimmer/position layers"]
    C --> D["4. create_chasers<br/>→ Beat-synced step sequences"]
    D --> E["5. create_collections<br/>→ Bundle into phases/moods"]
    E --> F["6. vc_create_pages<br/>→ Add show pages"]
    F --> G["7. vc_create_widgets<br/>→ Buttons, sliders, SoloFrames"]
    G --> H["8. vc_map_inputs<br/>→ Connect MIDI/OSC"]
    H --> I["9. vc_configure_feedback<br/>→ Set LED colors"]
    I --> J["10. vc_reflow_frame<br/>→ Auto-arrange layout"]

    style A fill:#e1f5fe
    style B fill:#e1f5fe
    style C fill:#e8f5e9
    style D fill:#e8f5e9
    style E fill:#e8f5e9
    style F fill:#fff3e0
    style G fill:#fff3e0
    style H fill:#f3e5f5
    style I fill:#f3e5f5
    style J fill:#fce4ec
```

**Ref:** Server instructions in [mcpserver.cpp](mcpserver.cpp) | Prompt `design_dj_show` in [prompts.cpp](tools/prompts.cpp)

---

## 13. Potential Issues & Wiring Analysis

```mermaid
flowchart TB
    subgraph "🔴 Memory Leak"
        ML1["mcpinit.cpp:31<br/>McpServer *server = new McpServer(...)"]
        ML2["mcpinit.cpp:27<br/>VCBridgeV5 *vcBridge = new VCBridgeV5(...)"]
        ML3["Neither object is ever deleted<br/>No parent QObject set"]
        ML1 --> ML3
        ML2 --> ML3
    end

    subgraph "🟡 ResourceManager Unused"
        RM1["mcpserver.cpp:39<br/>m_resourceManager(make_unique)"]
        RM2["Never has anything registered<br/>Empty manager passed to handler"]
        RM1 --> RM2
    end

    subgraph "🟡 FunctionManager Nullable"
        FM1["McpServer(doc, vcBridge,<br/>funcMgr = nullptr)"]
        FM2["registerFunctionTools(tm, doc, funcMgr)"]
        FM3{"Is funcMgr used<br/>inside function_tools.cpp?"}
        FM4["funcMgr parameter captured<br/>but only used in<br/>create_rgb_matrices for<br/>FunctionManager-specific ops"]
        FM1 --> FM2 --> FM3 --> FM4
    end

    subgraph "🟢 VC Null Guards — Correct"
        NG1["registerVCCreateTools: if (!vcBridge) return ✓"]
        NG2["registerVCUpdateTools: if (!vcBridge) return ✓"]
        NG3["registerVCInputTools: if (!vcBridge) return ✓"]
        NG4["registerVCLayoutTools: if (!vcBridge) return ✓"]
        NG5["vc_query_pages: if (vcBridge) { register } ✓"]
        NG6["vc_query_widgets: if (vcBridge) { register } ✓"]
    end

    subgraph "🟡 Tool Registration Comment Mismatch"
        CM1["io_tools.cpp:107<br/>Comment says 'query_midi_devices'<br/>but registers 'configure_plugin_params'"]
        CM2["Duplicate comment 'query_midi_devices'<br/>appears at both line 107 and line 165"]
        CM1 --> CM2
    end

    subgraph "🟡 const_cast Pattern"
        CC1["query_tools.cpp:95<br/>const_cast&lt;QLCFixtureDef*&gt;(def)->modes()"]
        CC2["Needed because modes() is non-const<br/>Could break if def is truly const"]
        CC1 --> CC2
    end

    subgraph "🟢 Thread Safety — Correct"
        TS1["All tool handlers wrapped in<br/>execOnMainThread(doc, [&]() {...})"]
        TS2["Uses Qt::BlockingQueuedConnection<br/>when threads differ"]
        TS3["try/catch prevents crashes"]
        TS1 --> TS2 --> TS3
    end

    style ML3 fill:#ffcdd2
    style RM2 fill:#fff9c4
    style FM4 fill:#fff9c4
    style CM2 fill:#fff9c4
    style CC2 fill:#fff9c4
    style NG1 fill:#c8e6c9
    style NG2 fill:#c8e6c9
    style NG3 fill:#c8e6c9
    style NG4 fill:#c8e6c9
    style NG5 fill:#c8e6c9
    style NG6 fill:#c8e6c9
    style TS3 fill:#c8e6c9
```

### Issue Detail Table

| # | Severity | Location | Issue | Impact |
|---|----------|----------|-------|--------|
| 1 | 🔴 High | [mcpinit.cpp:27-32](mcpinit.cpp) | `McpServer` and `VCBridgeV5` allocated with `new` but never `delete`d. No QObject parent set. | Memory leak for app lifetime. Minor since both live until app exit, but prevents clean shutdown. |
| 2 | 🟡 Medium | [mcpserver.cpp:39](mcpserver.cpp) | `ResourceManager` created but nothing is ever registered in it. | Dead code. No functional impact but adds confusion. |
| 3 | 🟡 Medium | [io_tools.cpp:107](tools/io_tools.cpp) | Comment says `// query_midi_devices` but the code registers `configure_plugin_params`. The actual `query_midi_devices` is registered at line 165 with the same comment duplicated. | Misleading for developers reading the code. |
| 4 | 🟡 Medium | [query_tools.cpp:95](tools/query_tools.cpp) | `const_cast<QLCFixtureDef*>(def)->modes()` — discards const to call `modes()`. | `modes()` should be const on `QLCFixtureDef`. Works but is a code smell. |
| 5 | 🟢 Info | [tool_registry.h](tools/tool_registry.h) | `validateFields()` uses linear scan over `initializer_list`. | O(n×m) complexity. Fine for small field counts (< 20) but not optimal. |
| 6 | 🟢 Info | [vcbridge.h](vcbridge.h) | `WidgetDetails` struct has ~40 fields for all widget types combined. | No runtime cost; sparse struct pattern is intentional. Could use variant/union for stricter typing. |
| 7 | 🟢 OK | All vc_*.cpp | All VC registration functions guard `if (!vcBridge) return;` | Correctly handles null VCBridge — tools simply won't be registered. |
| 8 | 🟢 OK | All tool handlers | All wrapped in `execOnMainThread()` | Thread safety between HTTP thread and Qt main thread is correct. |
| 9 | 🟢 OK | All create tools | Idempotent upsert pattern via `findFunction()` / `findWidgetByCaption()` | Safe to call repeatedly. |

---

## 14. File Reference Map

| File | Purpose | Lines | Tools Registered |
|------|---------|-------|-----------------|
| [mcpserver.h](mcpserver.h) | Server class declaration | ~40 | — |
| [mcpserver.cpp](mcpserver.cpp) | Server init, HTTP lifecycle | ~70 | Orchestrates all registration |
| [mcpinit.h](mcpinit.h) / [.cpp](mcpinit.cpp) | CLI options, startup wiring | ~35 | — |
| [vcbridge.h](vcbridge.h) | Abstract VC interface | ~700 | — |
| [vcbridgev5.h](vcbridgev5.h) / [.cpp](vcbridgev5.cpp) | QML VC implementation | ~2000 | — |
| [tools/tool_registry.h](tools/tool_registry.h) | Registration decls, `execOnMainThread`, `validateFields` | ~100 | — |
| [tools/conversions.h](tools/conversions.h) | JSON conversion functions | ~250 | — |
| [tools/idempotency.h](tools/idempotency.h) | Upsert helpers, name resolution, glob | ~100 | — |
| [tools/vc_tools_common.h](tools/vc_tools_common.h) | Widget type enum, field validation | ~400 | — |
| [tools/query_tools.cpp](tools/query_tools.cpp) | Query & patch tools | ~400 | 6: query_fixtures, query_available_fixtures, patch_fixtures, query_functions, vc_query_pages, vc_query_widgets |
| [tools/function_tools.cpp](tools/function_tools.cpp) | Function CRUD | ~800 | 8: create_scenes, create_chasers, create_sequences, create_efxs, create_collections, create_rgb_matrices, create_scripts, delete_functions |
| [tools/vc_create_tools.cpp](tools/vc_create_tools.cpp) | Widget creation | ~600 | 2: vc_create_pages, vc_create_widgets |
| [tools/vc_update_tools.cpp](tools/vc_update_tools.cpp) | Sparse widget updates | ~400 | 1: vc_update_widgets |
| [tools/vc_input_tools.cpp](tools/vc_input_tools.cpp) | Input mapping & feedback | ~400 | 3: vc_map_inputs, vc_configure_feedback, vc_set_key_sequences |
| [tools/vc_layout_tools.cpp](tools/vc_layout_tools.cpp) | Layout operations | ~350 | 4: vc_reparent_widgets, vc_delete_widgets, vc_detect_overlaps, vc_reflow_frame |
| [tools/io_tools.cpp](tools/io_tools.cpp) | I/O & universe config | ~500 | 9: configure_universes, configure_plugin_params, query_midi_devices, query_input_profiles, set_input_profile, query_feedback_profile, configure_osc, query_osc_status, configure_beat_source |
| [tools/channel_tools.cpp](tools/channel_tools.cpp) | Channel queries & config | ~300 | 5: query_fixture_channels, configure_channels, query_channel_modifiers, set_channel_modifiers, convert_degrees_to_dmx |
| [tools/prompts.cpp](tools/prompts.cpp) | MCP prompts | ~100 | 1 prompt: design_dj_show |
| [CMakeLists.txt](CMakeLists.txt) | Build configuration | ~50 | — |

**Total: ~34 tools, 1 prompt, 0 resources**

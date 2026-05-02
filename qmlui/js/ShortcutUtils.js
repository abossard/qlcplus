.pragma library

function isMac()
{
    return Qt.platform.os === "osx"
}

function display(sequence)
{
    if (!sequence)
        return ""

    if (!isMac())
        return sequence

    return sequence
        .replace(/Ctrl/g, "⌘")
        .replace(/Meta/g, "⌃")
        .replace(/Shift/g, "⇧")
        .replace(/Alt/g, "⌥")
        .replace(/Esc/g, "⎋")
        .replace(/\+/g, "")
}

function withShortcut(text, sequence)
{
    var shortcut = display(sequence)
    return shortcut ? qsTr("%1 (%2)").arg(text).arg(shortcut) : text
}

function isTextEditing(item)
{
    // Duck-type check: TextInput and TextEdit both have selectAll() and text property.
    // Cannot use instanceof in .pragma library scope (no QML type imports).
    return item && typeof item.selectAll === "function" && "text" in item
}

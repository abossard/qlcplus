import pytest

from edm_analysis.analyze import filename_tags


@pytest.mark.parametrize("name,expected", [
    ("Biscits - Freak  (Extended Mix) 2024 [128 B Minor].mp3", (128.0, "B Minor")),
    ("Andy C - Ghost (Extended Mix) 2022 [89 A Minor].mp3", (89.0, "A Minor")),
    ("Riverside vs Lick It (VIP Edit) 2022 [128 1A].mp3", (128.0, "1A")),
    ("Better Off Alone (Bootleg) 2015 [128 12B].mp3", (128.0, "12B")),
    ("Track [174.5 F# Major].mp3", (174.5, "F# Major")),
    ("No tags here.mp3", (None, None)),
])
def test_filename_tags(name, expected):
    assert filename_tags(name) == expected

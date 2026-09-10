"""Read-only tag adapter for the imported research tools.

The installed package implementation lives in tools/coo/package_read.py.
This adapter provides the tag-reading API; inventory scans still require the
original research reader supplied through those scripts' --reader-dir option.
"""
from pathlib import Path
import sys

TAG_BASE = 0x80800000
ENTRY_BITS = 13


class Reader:
    def __init__(self):
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'coo'))
        import package_read
        self._read = package_read.read

    def read_tag(self, tag):
        cls, data = self._read(tag)
        return data, cls

"""Tests of tools/feeds/school_holidays.py. Run: python3 -m unittest discover -s tools/feeds"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import school_holidays as sh  # noqa: E402

FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "openholidays-BY.json")


def load_fixture():
    with open(FIXTURE, encoding="utf-8") as f:
        return json.load(f)


class SchoolHolidayFeedTest(unittest.TestCase):
    def test_entries_from_fixture(self):
        entries = sh.entries_from_api(load_fixture())
        self.assertEqual(entries[2], ("2026-12-24", "2027-01-08", "Weihnachtsferien"))
        self.assertEqual(entries[1], ("2026-11-18", "2026-11-18", "Buß- und Bettag"))

    def test_ics_from_fixture(self):
        ics = sh.to_ics("BY", sh.entries_from_api(load_fixture()))
        self.assertIn("\r\n", ics)
        self.assertNotIn("\n", ics.replace("\r\n", ""))  # CRLF only
        self.assertTrue(ics.startswith("BEGIN:VCALENDAR\r\n"))
        self.assertTrue(ics.endswith("END:VCALENDAR\r\n"))
        self.assertIn("DTSTART;VALUE=DATE:20261224\r\n", ics)
        self.assertIn("DTEND;VALUE=DATE:20270109\r\n", ics)  # the day after the last day
        self.assertIn("SUMMARY:Weihnachtsferien\r\n", ics)
        self.assertIn("UID:ferien-BY-20261224@eink-dashboard\r\n", ics)
        self.assertIn("ODbL", ics)  # the data licence travels with the file
        self.assertEqual(ics.count("BEGIN:VEVENT"), 4)

    def test_failed_state_not_written(self):
        def fetch(state):
            if state == "BY":
                raise OSError("network down")
            return load_fixture() if state == "BW" else []

        with tempfile.TemporaryDirectory() as out:
            written = sh.build_all(out, fetch, states=["BY", "BW", "NI"])
            self.assertEqual(written, ["BW"])  # BY failed, NI had nothing: no file, the old one stays online
            self.assertEqual(sorted(os.listdir(out)), ["ferien-BW.ics"])


if __name__ == "__main__":
    unittest.main()

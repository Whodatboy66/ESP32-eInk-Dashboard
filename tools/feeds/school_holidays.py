#!/usr/bin/env python3
"""School holidays of the 16 German states as calendar files (stage 7, docs/plans/GIFT_EDITION.md).

Writes ferien-<STATE>.ics into the output folder; .github/workflows/feeds.yml publishes them on
the fixed pre-release "feeds", where the dashboards read them once a week
(src/services/SchoolHolidays.h). Data: OpenHolidays API (https://www.openholidaysapi.org),
ODbL 1.0; the files carry that note. A state whose download fails or has no entries gets no file,
so the last published one stays online.

Usage: python3 tools/feeds/school_holidays.py OUT_DIR
"""
import datetime
import json
import os
import sys
import urllib.parse
import urllib.request

STATES = ["BW", "BY", "BE", "BB", "HB", "HH", "HE", "MV", "NI", "NW", "RP", "SL", "SN", "ST", "SH", "TH"]
API = "https://openholidaysapi.org/SchoolHolidays"
NOTE = "Daten: OpenHolidays API (openholidaysapi.org), ODbL 1.0"


def fetch_state(state, today=None):
    """The API's answer (a list) for one state, from the start of this year to the end of next year."""
    today = today or datetime.date.today()
    query = urllib.parse.urlencode({
        "countryIsoCode": "DE", "subdivisionCode": "DE-" + state, "languageIsoCode": "DE",
        "validFrom": "%d-01-01" % today.year, "validTo": "%d-12-31" % (today.year + 1)})
    req = urllib.request.Request(API + "?" + query, headers={"accept": "application/json",
                                                              "user-agent": "eink-dashboard-feeds"})
    with urllib.request.urlopen(req, timeout=30) as r:
        data = json.load(r)
    if not isinstance(data, list):
        raise ValueError("unexpected answer")
    return data


def entries_from_api(data):
    """(first day, last day, name) per entry; the API's endDate is the last day (inclusive)."""
    out = []
    for e in data:
        names = e.get("name") or []
        name = next((n.get("text") for n in names if n.get("language") == "DE"), None)
        name = name or (names[0].get("text") if names else "Ferien")
        start, end = e.get("startDate"), e.get("endDate")
        datetime.date.fromisoformat(start)  # raises on garbage
        datetime.date.fromisoformat(end)
        out.append((start, end, name))
    out.sort()
    return out


def _ics_text(s):
    return s.replace("\\", "\\\\").replace(";", "\\;").replace(",", "\\,").replace("\n", "\\n")


def to_ics(state, entries):
    lines = ["BEGIN:VCALENDAR", "VERSION:2.0", "PRODID:-//eink-dashboard//school holidays//DE",
             "CALSCALE:GREGORIAN", "X-WR-CALNAME:Schulferien " + state, "X-WR-CALDESC:" + _ics_text(NOTE)]
    for start, end, name in entries:
        first = datetime.date.fromisoformat(start)
        after = datetime.date.fromisoformat(end) + datetime.timedelta(days=1)  # DTEND: the day after
        lines += ["BEGIN:VEVENT",
                  "UID:ferien-%s-%s@eink-dashboard" % (state, first.strftime("%Y%m%d")),
                  "DTSTAMP:%sT000000Z" % first.strftime("%Y%m%d"),
                  "DTSTART;VALUE=DATE:" + first.strftime("%Y%m%d"),
                  "DTEND;VALUE=DATE:" + after.strftime("%Y%m%d"),
                  "SUMMARY:" + _ics_text(name),
                  "TRANSP:TRANSPARENT",
                  "END:VEVENT"]
    lines.append("END:VCALENDAR")
    return "\r\n".join(lines) + "\r\n"


def build_all(out_dir, fetch=fetch_state, states=STATES):
    """Writes the files of the states that worked; returns their codes."""
    written = []
    for state in states:
        try:
            entries = entries_from_api(fetch(state))
        except Exception as e:  # noqa: BLE001 - one state failing must not stop the others
            print("%s: failed (%s), the published file stays" % (state, e))
            continue
        if not entries:
            print("%s: no entries, the published file stays" % state)
            continue
        with open(os.path.join(out_dir, "ferien-%s.ics" % state), "w", encoding="utf-8", newline="") as f:
            f.write(to_ics(state, entries))
        written.append(state)
        print("%s: %d entries" % (state, len(entries)))
    return written


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    os.makedirs(argv[1], exist_ok=True)
    written = build_all(argv[1])
    if not written:
        print("no state worked: nothing to publish")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

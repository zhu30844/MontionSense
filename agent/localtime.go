package main

import (
	"os"
	"regexp"
	"strconv"
	"strings"
	"time"
)

// The image carries no zoneinfo database and nothing exports TZ, so Go's
// time.Local resolves to UTC: it reads the TZ variable and /etc/localtime, and
// knows nothing of /etc/TZ, which is where uClibc and busybox look. The C
// daemon does read it, so it names recording directories in local time while
// the agent reasons about them in UTC. For eight hours a day -- everywhere the
// local date is already ahead of the UTC one -- that made the current day look
// like it was in the future, and lastRecordDate dropped it.
//
// Only the standard-time part of the POSIX string is honoured. A rule with DST
// transitions would leave the agent an hour off for half the year, which can
// still cross midnight, but a zoneinfo database is a poor trade for an image
// this size and no board this project targets is in such a zone. The offset
// sign is inverted by POSIX: AWST-8 is eight hours east of UTC.
var posixTZ = regexp.MustCompile(`^([A-Za-z]{3,})([+-]?)(\d{1,2})(?::(\d{2}))?(?::(\d{2}))?`)

func localFromEtcTZ(path string) *time.Location {
	raw, err := os.ReadFile(path)
	if err != nil {
		return nil
	}
	m := posixTZ.FindStringSubmatch(strings.TrimSpace(string(raw)))
	if m == nil {
		return nil
	}

	atoi := func(s string) int { n, _ := strconv.Atoi(s); return n }
	secs := atoi(m[3])*3600 + atoi(m[4])*60 + atoi(m[5])
	if m[2] != "-" {
		secs = -secs
	}
	return time.FixedZone(m[1], secs)
}

func init() {
	if loc := localFromEtcTZ("/etc/TZ"); loc != nil {
		time.Local = loc
	}
}

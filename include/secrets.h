/**
 * To prevent changes to this file once pulled and configured locally:
 *    git update-index --skip-worktree <path-to-file>
 *
 * To clear the skip-worktree bit:
 *    git update-index --no-skip-worktree <path-to-file>
 */

/*** Your WiFi Credentials ***/
const char *ssid = "your ssid";
const char *password = "your password";

/*** Your coordinates ***/
const float latitude = 00.00;
const float longitude = 00.00;
const float altitude = 100.00;
const int daysToLookup = 1;    // days ahead to look
const int minVisibility = 180; // seconds
static const char apiKey[] = "your-n2yo-api-key";
static const char ntpServerName[] = "pool.ntp.org";

/*** Your timezone ***/
// How to set: https://github.com/JChristensen/Timezone#coding-timechangerules
TimeChangeRule aEST = {"AEST", First, Sun, Apr, 3, 600};    // UTC + 10 hours
Timezone myTz(aEST);
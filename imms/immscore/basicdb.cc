#include <iostream>
#include <unistd.h>
#include <math.h>

#include "strmanip.h"
#include "immsdb.h"
#include "utils.h"

using std::endl;
using std::cerr; 

BasicDb::BasicDb()
{
    sql_set_pragma();
}

BasicDb::~BasicDb()
{
    try {
        Q q("DELETE FROM 'Journal' WHERE time < ?;");
        q << time(0) - 30 * DAY ;
        q.execute();
    }
    WARNIFFAILED();
}

void BasicDb::sql_set_pragma()
{
    try {
        QueryCacheDisabler qdb;

        Q("PRAGMA cache_size = 10000").execute();
        Q("PRAGMA synchronous = OFF;").execute();
        Q("PRAGMA temp_store = MEMORY;").execute();
    }
    WARNIFFAILED();
}

void BasicDb::sql_create_tables()
{
    QueryCacheDisabler qcd;
    RuntimeErrorBlocker reb;
    try
    {
        Q("CREATE TABLE 'Identify' ("
                "'path' VARCHAR(4096) UNIQUE NOT NULL, "
                "'uid' INTEGER NOT NULL, "
                "'modtime' TIMESTAMP NOT NULL, "
                "'checksum' TEXT NOT NULL);").execute();
                
        Q("CREATE TABLE 'Library' ("
                "'uid' INTEGER UNIQUE NOT NULL, "
                "'sid' INTEGER DEFAULT -1, "
                "'playcounter' INTEGER DEFAULT 0, "
                "'lastseen' TIMESTAMP DEFAULT 0, "
                "'firstseen' TIMESTAMP DEFAULT 0);").execute();

        Q("CREATE TABLE 'Rating' ("
                "'uid' INTEGER UNIQUE NOT NULL, "
                "'rating' INTEGER NOT NULL, "
                "'trend' INTEGER DEFAULT 0);").execute();

        Q("CREATE TABLE 'Acoustic' ("
                "'uid' INTEGER UNIQUE NOT NULL, "
                "'spectrum' TEXT, 'bpm' TEXT);").execute();

        Q("CREATE TABLE 'Info' ("
                "'sid' INTEGER UNIQUE NOT NULL," 
                "'artist' TEXT NOT NULL, "
                "'title' TEXT NOT NULL);").execute();

        Q("CREATE TABLE 'Last' ("
                "'sid' INTEGER UNIQUE NOT NULL, " 
                "'last' TIMESTAMP);").execute();

        Q("CREATE TABLE 'Journal' ("
                "'uid' INTEGER NOT NULL, " 
                "'delta' INTEGER NOT NULL, " 
                "'time' TIMESTAMP NOT NULL);").execute();
    }
    WARNIFFAILED();
}

int BasicDb::avg_rating(const string &artist, const string &title)
{
    try
    {
        if (title != "")
        {
            Q q("SELECT avg(rating) FROM Library "
                    "NATURAL INNER JOIN Info "
                    "INNER JOIN Rating ON Library.uid = Rating.uid "
                    "WHERE Info.artist = ? AND Info.title = ?;");

            q << artist << title;

            if (q.next())
            {
                int avg;
                q >> avg;
                return avg;
            }
        }

        if (artist != "")
        {
            Q q("SELECT avg(rating) FROM Library "
                    "NATURAL INNER JOIN Info "
                    "INNER JOIN Rating ON Rating.uid = Library.uid "
                    "WHERE Info.artist = ?;");
            q << artist;

            if (q.next())
            {
                int avg;
                q >> avg;
                if (avg)
                    return std::min(std::max(avg, 90), 115);
            }
        }
    }
    WARNIFFAILED();
    return -1;
}

bool BasicDb::check_artist(string &artist)
{
    try
    {
        Q q("SELECT artist FROM 'Info' WHERE similar(artist, ?);");
        q << artist;

        if (q.next())
        {
            q >> artist;
            return true;
        }
    }
    WARNIFFAILED();
    return false;
}

bool BasicDb::check_title(const string &artist, string &title)
{
    try
    {
        Q q("SELECT title FROM 'Info' WHERE artist = ? AND similar(title, ?)");
        q << artist << title;

        if (q.next())
        {
            q >> title;
            return true;
        }
    }
    WARNIFFAILED();
    return false;
}

void BasicDb::sql_schema_upgrade(int from)
{
    QueryCacheDisabler qcd;
    try 
    {
        AutoTransaction a;
        if (from < 6)
        {
            cerr << "IMMS: Direct upgrade from SCHEMA version < 6 unsupported"
                << endl << "Please upgrade to IMMS 1.2 first" << endl;
            exit(-1);
        }
        if (from < 7)
        {
            Q("DROP TABLE Acoustic;").execute();
        }
        if (from < 8)
        {
            // Backup the existing tables
            Q("CREATE TEMP TABLE Library_backup "
                    "AS SELECT * FROM Library;").execute();
            Q("DROP TABLE Library;").execute();

            // Backup the existing tables
            Q("CREATE TEMP TABLE Rating_backup "
                    "AS SELECT * FROM Rating;").execute();
            Q("DROP TABLE Rating;").execute();

            // Create new tables
            sql_create_tables();

            Q("INSERT INTO Rating (uid, rating) "
                    "SELECT uid, rating FROM Rating_backup;").execute();
            Q("DROP TABLE Rating_backup;").execute();

            Q("INSERT INTO Identify (path, uid, modtime, checksum) "
                    "SELECT path, uid, modtime, checksum "
                    "FROM Library_backup;").execute();

            Q("INSERT OR REPLACE INTO Library "
                    "(uid, sid, playcounter, lastseen, firstseen) "
                    "SELECT uid, sid, 10, ?, ? FROM Library_backup;")
                 << time(0) << time(0) << execute;

            Q("DROP TABLE Library_backup;").execute();
        }

        a.commit();
    }
    WARNIFFAILED();
}
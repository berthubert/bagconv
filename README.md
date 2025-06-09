# bagconv
Convert the official Dutch building/dwelling administration into a simpler format.
Also includes a small and fast web service for sharing this data.

These tools will allow you do straightforward queries to go from an address
to a postcode and vice-versa.  You also get the coordinates of an address,
and if you want, the contours of the buildings the address is in. Using
SQLite R*Tree tables, you can also quickly go from coordinates to an
address.

**STATUS: Pretty fresh - if you use this in production, do let me know (bert@hubertnet.nl)**

> Also, see this [related project](https://github.com/bartnv/xml-to-postgres/wiki/%5BNL%5D-LV-BAG-2.0-converteren)
> from [Bart Noordervliet](https://github.com/bartnv) which implements a more
> generic solution to convert the BAG (and other XML constructs) to PostgreSQL
> and PostGIS. Jan Derk [also wrote a tool](https://github.com/digitaldutch/BAG_parser) (in Python) that converts the BAG XML to SQLite.

It all starts out with the tremendously detailed set of XML files from the Dutch
land registry: [LV BAG with History](https://service.pdok.nl/lv/bag/atom/bag.xml).
The official documentation (in Dutch) can be found on [the GitHub page of
the Dutch Land Registry and Mapping
Agency](https://imbag.github.io/praktijkhandleiding/).

The BAG files contain data on:

 * Almost every Dutch building (a `Pand`), and its 2D shape
 * All recognized dwellings (`Verblijfsobject`), including mooring points
   for boats and assigned places for mobile homes, and its 2D location
 * All public spaces (`Openbare ruimte`) that could be a street address
 * All addresses assigned to the dwellings (`Nummeraanduiding`)

These four entities have a complicated relation. For example, a building
(`pand`) may contain multiple places of dwelling, but a place of dwelling
might also extend over multiple buildings. A building might also not be a
place of dwelling (a shed, for example). Also, a single place of dwelling
might have multiple addresses.

The XML files contain data on over 30 million objects, and their relations.
Oh, and the history. This is around 75GB of XML.

# Making life simpler
If you have downloaded and unzipped [the zip file containing zip files of the
XML
files](https://service.pdok.nl/lv/bag/atom/downloads/lvbag-extract-nl.zip)
you can run the code in this repository like this:

```bash
$ ./bagconv 9999{WPL,OPR,NUM,VBO,LIG,STA,PND}*.xml > txt
```

This will 1) create a text file with debugging output and 2) populate a
database called bag.sqlite

> Note: the WPL,OPR etc order is important for the tool to function
> correctly

If you run `sqlite3 bag.sqlite < mkindx`,  this will add useful indexes and
views.

This allows stuff like:

```
sqlite> select * from unilabel where straat ='Binnenhof' and huisnummer='19' and woonplaats="'s-Gravenhage";
|  straat   | huisnummer | huisletter | huistoevoeging |  woonplaats   | postcode |     x     |     y      | oppervlakte |  gebruiksdoel  
|-----------|------------|------------|----------------|---------------|----------|-----------|------------|-------------|----------------
| Binnenhof | 19         |            |                | 's-Gravenhage | 2513AA   | 81431.756 | 455218.921 | 7726        | kantoorfunctie 
Run Time: real 0.000 user 0.000000 sys 0.000681
```
(there are actually more fields which tell you the status of registrations,
plus the BAG identifiers, but I've omitted these here).

Incidentally, this is an example of of the tens of thousands of addresses
missing from the 'BAG Geopackage' which you can also download from the
official URL above.

The x and y coordinates are according to the [Dutch reference
grid](https://nl.wikipedia.org/wiki/Rijksdriehoeksco%C3%B6rdinaten). Also
included in the database are WGS84 coordinates.

The tables in this database correspond 1:1 to the BAG objects, where the
relations between entities are described via the intermediate tables
`vbo_num` (from `Verblijfsobject` (place of dwelling) to `Nummeraanduiding`
(address)) and `vbo_pnd` (from `Verblijfsobject` to `Pand` (building)).

The tables are:

 * `oprs` - public spaces, like streets and squares. 
 * `vbos` - places of dwelling, but also mooring points and mobile home
   locations. x and y are coordinates, in the Dutch specific coordinate
   system. Also, WGS84 latitude and longitude. In addition, includes an indication what this
   place of dwelling is for (housing, education, office etc)
 * `pnds` - `Panden` or buildings. Includes a 2D shape!
 * `nums` - `Nummeraanduidingen` - addresses, including postcode

In the `mkindx` file you'll find useful views that make querying easier.

# Geographical tables
If you run the SQL in `geo-queries`, a new table gets populated using the
SQLite R*Tree module. This table (geoindex) can be queried rapidly to find
`vbos` within certain x and y coordinates, or within certain longitudes and
lattitudes. Use the `vbo_id` field to find associated places of dwelling.

Sample query:

```SQL
select x as rdX, y as rdY,straat,woonplaats,postcode, (lat-?)*(lat-?)+(lon-?)*(lon-?) as deg2dist 
from geoindex,alllabel where alllabel.vbo_id = geoindex.vbo_id and 
minLat > ? and maxLat < ? and minLon > ? and maxLon < ? 
order by deg2dist asc limit 10
{lat, lat, lon, lon, lat-0.005, lat+0.005, lon-0.005, lon+0.005})
```

The question marks need to be replaced with the things between {curly
braces} below the SQL.

# Validity periods
When comparing the output of this tool to commercial offerings or the [excellent live official database](https://bagviewer.kadaster.nl/lvbag/bag-viewer/index.html), you can find small discrepancies, mostly related to the validity period.

The montly extract of the BAG pre-announces changes that will happen in the (near) future. When `emlconv` runs, it checks the validity period of all entries against the current date, and then emits to the CSV file and SQLite database data that is valid at that time. 

This means that if you regenerate the CSV file and database after a few weeks, the contents will be different. Conversely, if you do not regenerate, the output created earlier will list data that is by now invalid.

Addresses which have existed, but are no longer valid, or addresses which will exist, are stored in `inactnums`.

# Some painful words on case
In the database, houseletters can be both lowercase and uppercase. Some streets even have both upper and lower case letters. Users of addresses are mostly not aware if an address has an upper or lower case letter. However, the lower and upper case are officially different. 

To deal with this, the sqlite database is provisioned with 'collate nocase' for huisletter and huistoevoeging (the additional extra field). This means that lookups for `huisletter='a'` and `huisletter='A'` deliver the same result. 

# Some examples

This gets you everything for the headquarters of the Dutch Kadaster agency, including the shape of their building:
```sql
sqlite> select * from alllabel where straat='Hofstraat' and huisnummer=110 and woonplaats='Apeldoorn';
        straat = Hofstraat
    huisnummer = 110
    huisletter = 
huistoevoeging = 
    woonplaats = Apeldoorn
      postcode = 7311KZ
             x = 194315.783
             y = 469449.074
           lon = 5.96244253360916
           lat = 52.2117344207437
   oppervlakte = 8870
gebruiksdoelen = ["kantoorfunctie"]
      bouwjaar = 1985
    num_status = Naamgeving uitgegeven
    vbo_status = Verblijfsobject in gebruik
      vbo_type = vbo
        num_id = 0200200000007079
        vbo_id = 0200010000090244
        opr_id = 0200300022471548
        pnd_id = 0200100000001088
```

If you just want to get from postcode to address:

```sql
sqlite> select * from unilabel where postcode='7311KZ' and huisnummer=110;
        straat = Hofstraat
    huisnummer = 110
    huisletter = 
huistoevoeging = 
    woonplaats = Apeldoorn
      postcode = 7311KZ
             x = 194315.783
             y = 469449.074
           lon = 5.96244253360916
           lat = 52.2117344207437
   oppervlakte = 8870
gebruiksdoelen = ["kantoorfunctie"]
    num_status = Naamgeving uitgegeven
    vbo_status = Verblijfsobject in gebruik
      vbo_type = vbo
        num_id = 0200200000007079
        vbo_id = 0200010000090244
        opr_id = 0200300022471548
```


# Compiling
Make sure you have cmake and SQLite development files installed, and then run:

```bash
cmake .
make
```

# The http server
If you run `bagserv 2345`, you can send it the following queries:

 * http://0.0.0.0:2345/7311KZ/110 - all addresses with postcode 7311KZ and
   house number 110
 * http://0.0.0.0:2345/7311KZ/110/A - all addresses with postcode 7311KZ and
   house number 110 and house letter A
 * http://0.0.0.0:2345/7311KZ - all addresses with postcode 7311KZ
 * http://0.0.0.0:2345/52.2117344207437/5.96244253360916 - the address
   closest to 52.2117344207437N, 5.96244253360916E (WGS84)

The answer in all cases is straightforward JSON with one or more addresses
in there:

```JSON
[
  {
    "bouwjaar": "1985",
    "gebruiksdoelen": ["kantoorfunctie"],
    "huisletter": "",
    "huisnummer": "110",
    "huistoevoeging": "",
    "lat": "52.2117344207437",
    "lon": "5.96244253360916",
    "num_status": "Naamgeving uitgegeven",
    "oppervlakte": "8870",
    "straat": "Hofstraat",
    "vbo_status": "Verblijfsobject in gebruik",
    "woonplaats": "Apeldoorn"
  }
]
```

Often, the testing instance on
[https://berthub.eu/pcode/](https://berthub.eu/pcode/52.2117344207437/5.96244253360916) will be
active. Send it queries like
[https://berthub.eu/pcode/7311KZ/110](https://berthub.eu/pcode/7311KZ/110).

Note, if you want to run this service yourself, and use nginx as a front
proxy, you need to have this in your server stanza: "merge_slashes off;".
Otherwise URLs like https://yourserver/9728KX/156//2 don't work (this is a
house without a house letter, but with a house toevoeging).


# Fun testing addresses

 * Binnenhof 19, 's-Gravenhage: One VBO with several addresses (Nummerindicaties)
 * Schiedamseweg 56, Rotterdam: One VBO, one Nummerindicatie but 5 buildings
   (PNDs)

# Docker
Building the Docker image requires at least 120GB of free disk space 
during build time and can take up to 15 minutes to build. The resulting
`bag.sqlite` and docker image combined will be approximately 10GB in size.

Please make sure you are using a [recent](https://docs.docker.com/desktop/install/linux-install/) 
Docker engine version (for example 24.0.5 or newer).

```
$ docker build -t bagconv:latest .
$ docker run -p 1234:1234 bagconv:latest

$ curl -s http://127.0.0.1:1234/7311KZ/110 | jq
[
  {
    "bouwjaar": "1985",
    "gebruiksdoelen": ["kantoorfunctie"],
    "huisletter": "",
    "huisnummer": "110",
    "huistoevoeging": "",
    "lat": "52.2117344207437",
    "lon": "5.96244253360916",
    "num_status": "Naamgeving uitgegeven",
    "oppervlakte": "8870",
    "rdX": "194315.783",
    "rdY": "469449.074",
    "straat": "Hofstraat",
    "vbo_status": "Verblijfsobject in gebruik",
    "woonplaats": "Apeldoorn"
  }
]
```

To clean up build artifacts you can (after running `docker run bagconv:latest` at least once) 
run the following command. Warning: this will remove all cache and dangling images and containers 
from your host. This should free up approximately 110GB of space on your machine.

```
docker system prune
```

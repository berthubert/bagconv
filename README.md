# bagconv
Convert the official Dutch building/dwelling administration into a simpler format.

This will allow you do straightforward queries to go from an address to a postcode and vice-versa. You also get the coordinates of an address, and if you want, the contours of the buildings the address is in.

**STATUS: Very fresh, do NOT rely on this code for anything serious yet!**

> Also, see this [related project](https://github.com/bartnv/xml-to-postgres/wiki/%5BNL%5D-LV-BAG-2.0-converteren)
> from [Bart Noordervliet](https://github.com/bartnv) which implements a more
> generic solution to convert the BAG (and other XML constructs) to PostgreSQL
> and PostGIS.

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

Incidentally, this is one of the tens of thousands of addresses missing from
the 'BAG Geopackage' which you can also download from the official URL
above. 

The x and y coordinates are according to the [Dutch reference
grid](https://nl.wikipedia.org/wiki/Rijksdriehoeksco%C3%B6rdinaten).

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
  gebruiksdoel = kantoorfunctie
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
  gebruiksdoel = kantoorfunctie
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

# Fun testing addresses

 * Binnenhof 19, 's-Gravenhage: One VBO with several addresses (Nummerindicaties)
 * Schiedamseweg 56, Rotterdam: One VBO, one Nummerindicatie but 5 buildings
   (PNDs)


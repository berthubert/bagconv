# bagconv
Convert the official Dutch building/dwelling administration into a simpler format

**STATUS: Very fresh, do NOT rely on this code for anything serious yet!**

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
If you have 16 gigabytes of ram, and you have downloaded and unzipped [the
XML
files](https://service.pdok.nl/lv/bag/atom/downloads/lvbag-extract-nl.zip)
you can run the code in this repository like this:

```bash
$ ./bagconv 9999{OPR,NUM,WPL,VBO,LIG,STA,PND}*.xml > txt
```

This will 1) create a text file with debugging output and 2) populate a
database called bag.sqlite

If you run `sqlite3 bag.sqlite < mkindex`,  this will add useful indexes and
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
   locations. x and y are coordinates. Also includes an indication what this
   place of dwelling is for (housing, education, office etc)
 * `pnds` - `Panden` or buildings. Includes a 2D shape!
 * `nums` - `Nummeraanduidingen` - addresses, including postcode

In the `mkindex` file you'll find useful views that make queryig easier.

# Compiling
Make sure you have cmake and SQLite development files installed, and then run:

```bash
cmake .
make
```


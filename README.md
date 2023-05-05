# bagconv
Convert Dutch building/dwelling administration into a simpler format

Starts out with the tremendously detailed set of XML files from the Dutch
land registry: [LV BAG with
History](https://service.pdok.nl/lv/bag/atom/bag.xml).

Run the code like this:

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
```

Incidentally, this is one of the tens of thousands of addresses missing from
the 'BAG Geopackage' which you can also download from the official URL
above. 

# Compiling
Make sure you have cmake and SQLite development files installed, and then run:

```bash
cmake .
make
```

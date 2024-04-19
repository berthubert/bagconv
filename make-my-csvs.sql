.header on
.mode csv
.output postcodes-nl-geo.csv

select straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode,CASE WHEN x = -1 THEN '' ELSE CAST(round(x) as INT) END as rd_x,CASE WHEN x = -1 THEN '' ELSE CAST(round(y) as INT) END as rd_y
from unilabel 
where postcode!= '' 
order by postcode,huisnummer,huisletter,huistoevoeging;

.output postcodes-nl.csv
.header on

select straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode
from unilabel 
where postcode!= '' 
order by postcode,huisnummer,huisletter,huistoevoeging;

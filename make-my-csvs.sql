.header on
.mode csv
.output postcodes-nl-geo.csv

select straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode,CAST(round(x) as INT) as rd_x,CAST(round(y) as INT) as rd_y
from unilabel 
where postcode!= '' 
order by postcode,huisnummer,huisletter,huistoevoeging;

.output postcodes-nl.csv
.header on

select straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode
from unilabel 
where postcode!= '' 
order by postcode,huisnummer,huisletter,huistoevoeging;

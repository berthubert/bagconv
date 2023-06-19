.header on
.mode csv
.output pcodes-geo.csv

select straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode,x,y,lon,lat,oppervlakte,gebruiksdoel 
from unilabel 
where postcode!= '' 
order by postcode,huisnummer,huisletter,huistoevoeging;

.header off
select straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode,x,y,lon,lat,oppervlakte,gebruiksdoel 
from unilabel 
where postcode== '' 
order by postcode,huisnummer,huisletter,huistoevoeging;

.output pcodes.csv
.header on

select straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode,oppervlakte,gebruiksdoel 
from unilabel 
where postcode!= '' 
order by postcode,huisnummer,huisletter,huistoevoeging;

.header off
select straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode,oppervlakte,gebruiksdoel 
from unilabel 
where postcode== '' 
order by postcode,huisnummer,huisletter,huistoevoeging;

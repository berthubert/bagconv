
create index vbo_pnd_idx1 on vbo_pnd(pnd);
create index vbo_pnd_idx2 on vbo_pnd(vbo);

create index vbo_num_idx1 on vbo_num(num);
create index vbo_num_idx2 on vbo_num(vbo);

create index vbos_id on vbos(id);
create index pnds_id on pnds(id);
create index nums_id on nums(id);
create index oprs_id on oprs(id);
create index wpls_id on wpls(id);

create index adridx on nums(postcode,huisnummer,huisletter,huistoevoeging);
create index woonplaatsstraatidx on nums(woonplaats);

create index ligtaanidx on nums(ligtAanRef);
create index straatnaamidx on oprs(naam);

CREATE INDEX xindex on vbos(x);
CREATE INDEX yindex on vbos(y);

create view unilabel as select oprs.naam as
straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode,x,y,lon,lat,oppervlakte,gebruiksdoelen,nums.status
as num_status, vbos.status as vbo_status, vbos.type as vbo_type,nums.id as num_id, vbos.id as
vbo_id, nums.ligtAanRef as opr_id from nums,oprs,vbos,vbo_num where
nums.id=vbo_num.num and nums.ligtAanRef = oprs.id and vbo_num.vbo=vbos.id and num_status!='Naamgeving ingetrokken';


create view alllabel as select oprs.naam as
straat,huisnummer,huisletter,huistoevoeging,woonplaats,postcode,x,y,lon,lat,oppervlakte,gebruiksdoelen,bouwjaar,nums.status
as num_status, vbos.status as vbo_status, vbos.type as vbo_type,nums.id as num_id, vbos.id as
vbo_id, nums.ligtAanRef as opr_id, pnds.id as pnd_id from nums,oprs,vbos,vbo_num,vbo_pnd,pnds where
nums.id=vbo_num.num and nums.ligtAanRef = oprs.id and vbo_num.vbo=vbos.id and num_status!='Naamgeving ingetrokken' 
and vbos.id=vbo_pnd.vbo and pnds.id=vbo_pnd.pnd;

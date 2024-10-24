DB=lapolar
cd /respaldo

echo "HOME:$HOME"
echo "ORIONDIR:$ORIONDIR"

#dbaccess lapolar@polart_tcp <<! 
echo "Conectando a $DB..."

TMP=`mktemp`
dbaccess $DB <<! > $TMP 2>>/dev/null

SELECT IncDate(vafereal, -730)
FROM tinvalper, tgndepende
WHERE vaestado = 'A'
AND declase = 'A'
AND vadepend = dedepend
!

FECHATOPE=`tail -2 $TMP | head -1`
rm -f $TMP

dbaccess $DB <<! 

set lock mode to wait;
    
SELECT "Emitiendo viajero.txt desde $FECHATOPE ..." FROM tgnparamet;

unload to viajero.txt
SELECT 
Maximo(octcambio,1), 
tcmordenco.ocnitcc,
occodniv,
panivel02,
armodelo,
ChgNullS(ChgNullS(ocfecemb,arfecemb), "00000000"), 
"00000000", "00000000", "00000000",
ChgNull(occostous,0), 
SUM(odcanped),paarticul,ocorden,artempora,ocfecord,
ChgNullI(tcmordenco.octempora,0), composicion(paarticul),
ChgNullS(tcmordenco.ocestado, ' '),
ChgNull((SELECT oafacpre1 FROM tcvordadic WHERE oanumlote=ocnumlote AND oaconsec=occonsec),0),
ChgNull((SELECT oafacmar1 FROM tcvordadic WHERE oanumlote=ocnumlote AND oaconsec=occonsec),0),
ChgNull((SELECT oafacpue1 FROM tcvordadic WHERE oanumlote=ocnumlote AND oaconsec=occonsec),0),
ChgNull((SELECT oadolar FROM tcvordadic WHERE oanumlote=ocnumlote AND oaconsec=occonsec),0),
ChgNull(ocpreciovta,0),ChgNull(occoleccion,0),ChgNullS(ocfecing,''),ChgNullS(ocfecfin,''),
ChgNull(octotalunc,0),ChgNull((occostous*occostocol)/100,0),
nvl(occolgado, 0), nvl(ocplanchado, 1), nvl(occajatarea, "N"),
nvl(ocunipack, "UNI"), nvl(ocunicaja, "UNI"), nvl(oainnerpacks, 0)
FROM tcmordenco, tcmordende, tgnnitster, tmpnaartplu, tmparticul,
OUTER (tcvordenco, tcvordadic)
WHERE ntnitcc = tcmordenco.ocnitcc  
AND ocnumlote = oanumlote
AND occonsec = oaconsec
AND ntorigen = 'E'    
AND paplu = odplu   
AND ararticul = paarticul   AND ararticul=ocarticul
AND odnumord = ocorden    
AND ocfecord > "$FECHATOPE"   
AND oddepend = tcmordenco.ocdepend  
AND ocnumord > 0    
AND ocnumord = ocorden    
AND occonsecbb=0
GROUP BY 1,2,3,4,5,6,7,8,9,10,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34;

SELECT "Emitiendo viajerod.txt ..." FROM tgnparamet;
UNLOAD TO viajerod.txt    
SELECT ocorden, licodigo,lidescrip,panivel02,
(SELECT a4.nadescrip FROM tmpnivagr a4 WHERE a4.naclave=20000+panivel02),  
paatrib01,a1.atdescrip,paatrib02,a2.atdescrip,mamarca,madescrip,
ChgNullS((SELECT tidescrip FROM tgntiposdf WHERE titipo=30 AND ticodigo=ocpuerto),'NA'),
odpreclst/Maximo(octcambio,1),a.paplu,SUM(odcanped),
(case papais when 1 then (SELECT ChgNullS(max(pbcodibar),'') FROM tmpplubarr WHERE pbplu=a.paplu AND pbtipocod='I')
else (select ChgNullS(max(pbcodibar),(SELECT ChgNullS(max(pbcodibar),'') FROM tmpplubarr WHERE pbplu = a.paplu and pbtipocod = 'O')) from tmpplubarr where pbplu = a.paplu and pbtipocod = 'I') end)
FROM tcmordenco,tcmordende,tgnnitster, tmpnaartplu a, tmparticul, 
OUTER tcvordenco,tgnlineas,tmppluatri b, tgnatribut a1, tgnatribut a2,  
tgnmarcasp,tgnparamet    
WHERE ntnitcc=tcmordenco.ocnitcc  
AND ntorigen='E'    
AND a.paplu=odplu   
AND ararticul=paarticul 
AND ararticul=ocarticul
AND odnumord=ocorden    
AND ocfecord>'$FECHATOPE'
AND oddepend=tcmordenco.ocdepend  
AND ocnumord>0    
AND ocnumord=ocorden    
AND licodniv=panivel01    
AND a.paplu=b.paplu   
and a1.atatribut=paatrib01
and a2.atatribut=paatrib02
and armarca=mamarca
and occonsecbb=0 
GROUP BY 1,2,3,4,5,6,7,8,9,10,11,12,13,14,16;

SELECT "Emitiendo viajeroc.txt ..." FROM tgnparamet;
UNLOAD TO viajeroc.txt   
SELECT ocnumord,ocarticul,b.odplu,b.odtalla,a.oddescrip,MAX(a.odcantidad),SUM(b.odcantidad)
FROM tcvordende a,tcvordende b,tcvordenco
WHERE a.odtipo=3 
AND b.odtipo=2
AND a.odnumlote=b.odnumlote AND a.odnumlote=ocnumlote AND b.odnumlote=ocnumlote
AND a.odconsec=b.odconsec AND a.odconsec=occonsec AND b.odconsec=occonsec
AND a.odtalla=b.odtalla
AND ocfecing>'$FECHATOPE'
GROUP BY 1,2,3,4,5
ORDER BY 1,2,4;

SELECT "Emitiendo viajeroe.txt ..." FROM tgnparamet;
UNLOAD TO viajeroe.txt   
SELECT ocnumord,odplu,odcantidad,odembarque, IncDate(odembarque, tivalor*(-1))
FROM tcvordende,tcvordenco, tgntiposdf
WHERE odtipo=4
AND titipo = 81
AND ticodigo = 1
AND ocnumlote=odnumlote
AND occonsec=odconsec
AND ocfecing>'$FECHATOPE'
ORDER BY 1,2,3;

!

echo "Enviando a clientes..."
echo cd /yo/conversion/prog >viajero.tmp  
echo ascii >>viajero.tmp    
echo put /respaldo/viajero.txt viajero.txt>>viajero.tmp   
echo put /respaldo/viajerod.txt viajerod.txt>>viajero.tmp
echo put /respaldo/viajeroc.txt viajeroc.txt>>viajero.tmp
echo put /respaldo/viajeroe.txt viajeroe.txt>>viajero.tmp           
ftp nodohp01 <viajero.tmp  

echo "Enviando a workflow..."
cd /respaldo
#ftp -n workflow << EOF 
#user EMPRESASLAPOLAR\uquality UqualityFTP
ftp -n nodohp01 << EOF 
user Anonymous xx
ascii
put viajero.txt
put viajerod.txt
put viajeroc.txt
put viajeroe.txt
quit
EOF

echo "Listo."

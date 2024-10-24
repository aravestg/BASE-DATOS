
/************************************************************************
*                               RRcAprRec.c                            *
* Descripcion: Aprueba un recibo de mercaderia                          *
*                                                                       *
* Algoritmo:   (si es necesario)                                        *
*                                                                       *
* VIENE UNA LISTA DE LAS MODIFICACIONES QUE SE HAN HECHO                *
* Fecha        Descripcion                              Responsable     *
* 31-Ene-95    Creacion Programa                        FAM             *
*                                                                       *
*************************************************************************/

////////////////////////////////////////////////////////////////////////////////////////////////////
// FECHA        Autor   Track   Comentarios
////////////////////////////////////////////////////////////////////////////////////////////////////
// 11-Mar-2009  GPL     388     Agregar parámetro para generar o no reporte de Distribución.
// 27-Mar-2009	PRC		396		Cargar las unidades recibidas en la cita de o los lotes recibidos.
// 17-Jun-2009  MVM  INCIDENCIA	No debe incrementar IVA para costo IFRS de productos importados.
// 30-Jul-2009  PRC  	477		Pareo automático de las cajas con mercadería de importación.
// 01-Abr-2013	PRC	Ticket-78889	Se habilita el área 26 para generar caja única. Línea 29.
// 31-Dic-2015  MVM     00      Para filtra costo comex segun Ejerc/Embarq/Correl.
// 14-Feb-2018  MVM     00      Funcionalidad para VEV.
// 23-Nov-2018  MVM     00      Se compromete inventario(incomprcli) a cliente cuando se reciba ASN VeV XD.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ApplStd.h>
#define  MAXREGS 1000
#define  CCOMMIT  200000
#define  MAXROW   66             /* Numero de Filas Maximas Permitidas */
#define  MAXCOL  120             /* Numero de Columnas Maximas Permitidas */
#define  AREA_NO_CAJAUNICA 	"8,9,10,25,29,46,47,61"
#define  TIPO_AREA_SIN_CAJA 72
#define CHILE 1
#define LINE131 "--------------------------------------------------------------------------------------------------------------------------------------"
#define COL_MARK "|" 

extern char  gc_stmsql[];        /* Sentencia SQL */
extern int   gi_error_sql;       /* Codigo error SQL */
static char  dummy_str[150];     /* Para guardar cadenas a imprimir */
static int   gi_PacketID;

/************************************************************************
*       SE DECLARAN SOLO LAS QUE USA ESTE PROGRAMA                      *
*************************************************************************/

static struct tinventa    *gprinventa; 
static struct tinvalper   *gprvalper; 
static struct tcmestprov  *gprestprov; 
static struct trcrecdet   *gprrecdet;
static struct tintrans    *gprtrans;
static struct tgnparamet  *gprparamet;   /* Estructura a la tabla tgnparamet */
static struct trcrecibos  *gprrecibos[200];
static struct tdbordendc  gprordendc[MAXREGS];


int           numfecha;
int		gi_cuenta, gi_contcta;
DEPEND        gi_depend, gi_depdes;
CONSEC        gi_orden, gi_ordend;
CONSEC        gi_ordcomp;
TIPOREC       gs_tiporec[TIPOREC_L];
NIT           gs_nitcc[NIT_L];
NIVEL_A       gi_nivel;
ID_NIVEL      gi_codniv, gi_codniv1;
int           gi_factorph;
TIME          gs_tiempo[TIME_L];
FECHA         gs_fecha[FECHA_L];
FECHA         gs_horarec[FECHA_L];
FECHA         gs_fecord[FECHA_L];
char          gs_anden[5];
FECHA         gs_fecharealperiodo[FECHA_L];
USUARIO       gs_usuario[USUARIO_L];
SWITCH        gc_parcial, gc_tipotras;
int			  gi_nserie;
SWITCH        gc_swrec;
VALAL         gd_cosrech, gd_cosrec;
VALAL         gd_canpedida, gd_vrpresup;
static        char gs_nomrep[NAMERPT];
static        int rep1;
int           gi_codtran, gi_codiva; /* variable para el nuvo log */
    int      gi_ordcomph;     /* Orden de compra hijo */
int				gi_num_oc, gi_ordComex;
static VALAL  gd_compen;
static VALAL  gd_totfac;
static VALAL  gd_totrec;
static int    gi_maxlin;            /* Maximo de lineas para el reporte */
static int    gi_marlin;            /* Margen para pie de pagina        */
static NOMDEP gs_dep[NOMDEP_L];     /* Nombre de la dependencia */
       double gd_cantdesp;
static char   gc_modrecep;

char gc_dctipdoc;
int  gi_dcdocum;
double  gd_dcmontobruto; 
char gc_dcfemision[20];
char gc_dcformato[20];			
char gc_dctipotrans;
int  gi_dcimpuesto;
int  gi_dccodmoneda; 
char gc_dccodcausal[20];
char gc_dcobservaciones[100];	
int  report1;
int  gi_credId, gi_ejerc, gi_embcorr;

/**************************************************************************/
static int
FN_Rcplhijo(ARTICULO pi_plu)
{
    ARTICULO li_plhijo;
	
    gi_factorph = 1;
    return(0);		// Se debe recibir el Padre e Hijos
    
    // Selecciona PLU hijo si existe y obtiene el factor padre-hijo
    sprintf(gc_stmsql, "SELECT plplu, plfactph \
FROM tmpplus \
WHERE plpadre = %d AND plplu != plpadre",
			pi_plu);
	
    gi_error_sql = DB_exec_sql(MEZDB, gc_stmsql);
        
    if (gi_error_sql == 0) {
        li_plhijo = 0;
        return(li_plhijo);
    }
    else if (gi_error_sql < 0) {
        DB_check_error(gi_error_sql);
        return;
    }
    else {
        DB_int_get_field(1, &li_plhijo);
        DB_int_get_field(2, &gi_factorph);
    }
    return(li_plhijo);

}

/************************************************************************
*                         PR_Encabe_Error()                                 *
*                                                                       *
* Descripcion :  Imprime Encabezado de Errores del Reporte              *
*                                                                       *
*************************************************************************/
static void
PR_Encabe_Error()
{
    PR_ImpEncab(rep1,"RRcAprRec", 70,
				"APROBACION DE RECIBOS", 25, gs_dep);
    skip_lines(rep1,2);
	
    return;
}
//////////////////////////////////////////////////////////////////////
//							RecibidoEnCita							//
// Proposito : Cargar las unidades recibidas en la cita del lote.	//
// Fecha : 27-Mar-2009		Autor : PRC		Track : 396				//
//////////////////////////////////////////////////////////////////////
static int RecibidoEnCita()
{
	NRORDEN iOrden;
	
	sprintf(gc_stmsql, "UPDATE trcdcitab2b SET edcanrec = nvl((SELECT SUM(rdcanrec) \
FROM trcrecdet WHERE rdorden = %d AND rdtipo = '%s' AND rddepend = %d AND rdplu = edplu), 0) \
WHERE ednumlote IN (SELECT UNIQUE pnlote FROM trcpacklisn, trcrecibos \
WHERE rcorden = %d AND rctipo = '%s' AND rcdepend = %d \
AND pnordenrec = %d AND pntiporec = '%s' AND pnordenoc = rcordenc)",
			gi_orden, gs_tiporec, gi_depend,
			gi_orden, gs_tiporec, gi_depend,
			gi_orden, gs_tiporec);
	
	gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);
	if (gi_error_sql < 0)
	{
		DB_check_error(gi_error_sql);
		return(gi_error_sql);
	}
	return(1);
}

static int 
CumplimientoVentaEnVerde(DEPEND pi_depend, ARTICULO pi_plu, CANTINV pd_totrec, OBSERV ps_tipoflujo[OBSERV_L],
                         CONSEC pi_ordenRec, TIPOREC ps_tiporec[TIPOREC_L])
{
	int		li_idx, li_regs;
	CANTINV ld_cantrec = pd_totrec;
	struct ST_Plus
	{
		CONSEC		despacho;
		CANTINV		cant;
	} Despachos[10000];
	
	//21-09-2023. Solo debe marcar ND cuando Verificacion de ASN sea tipo VEV.
	if ( strcmp(ps_tipoflujo, "VEV") == 0 )
	{
	    // Busca ND para actualizar la fecha de recibo en CD.
		sprintf(gc_stmsql, "SELECT (select MAX(CAST(batch_nbr AS INT)) from logfireSVD where file_name = rcobserv2) nd \
FROM trcrecibos, trcrecdet \
WHERE rcdepend = %d \
AND rcorden = %d \
AND rctipo = '%s' \
AND rddepend = rcdepend \
AND rdorden = rcorden \
AND rdtipo = rctipo \
AND rdplu = %d", pi_depend, pi_ordenRec, ps_tiporec, pi_plu);
	
		gi_error_sql = DB_exec_sql (MEZDB, gc_stmsql);
		
		if (gi_error_sql < 0 )		
		{
			DB_check_error(gi_error_sql);
			return(gi_error_sql);
		}
		li_regs = gi_error_sql;
		
		if ( li_regs > 0)
		{
			if (DB_int_get_field(1, &Despachos[0].despacho) < 0) return(-1);
		
			sprintf(gc_stmsql, "UPDATE tdscedespa SET dcfecrecb = '%s' \
WHERE dcdepend = %d AND dcnumdes = %d", gprvalper->vafereal, pi_depend, Despachos[0].despacho);

			gi_error_sql = DB_exec_sql (MEZDB, gc_stmsql);
				
			if (gi_error_sql < 0 )
			{
				DB_check_error(gi_error_sql);
				return(-1);
			}
			
			// Debe comprometer und. a clientes para enlazar el ND con el Inventario en CD.
			sprintf(gc_stmsql, "UPDATE tinventa SET \
incomprcli = incomprcli + %.2f \
WHERE inplu = %d \
AND independ = %d", ld_cantrec, pi_plu, pi_depend);

			gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );

			if (gi_error_sql < 0 )
			{
				DB_check_error(gi_error_sql);
				return(-1);
			}			
		}
	}
	
	return(1);
}

/************************************************************************
*                       FN_RcNivel2                                   * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Obtiene el nivel 1 de un nivel de agregacion dado      *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 21-Mar-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static int
FN_RcNivel1( NIVEL_A pi_nivel, ID_NIVEL pi_codniv)
{
    if (pi_nivel != 1)
    {
        sprintf(gc_stmsql, "SELECT nanivel01 FROM tmpnivagr \
WHERE naclave = (%d * 10000) + %d",
                            pi_nivel, pi_codniv);

        gi_error_sql = DB_exec_sql (MEZDB, gc_stmsql);

        if (gi_error_sql < 0 )
        {
            DB_check_error( gi_error_sql);
            return;
        }
        else
        if (gi_error_sql == 0)
        {
            abort_trans(RECDB);
            set_ret_val(NOT_FOUND);
            return;
        }
        else
        if ( DB_int_get_field( 1,&pi_codniv) < 0 ) return; 
    }
    return (pi_codniv);
}

/************************************************************************
*                       PR_RcActFab                                     * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Actualiza la tabla de TCMORDENCO                       *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 13-Feb-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static void
PR_RcActFab(DEPEND pi_depend, CONSEC pi_orden, TIPOREC ps_tiporec[TIPOREC_L])
{

    sprintf(gc_stmsql,"UPDATE tinordfab SET ofestado = 'Z', \
time_stmp ='%s', offecha = '%s' \
WHERE ofdepend = %d  \
AND ofestado = 'X' \
AND ofconsecu = ( SELECT rcordenc FROM trcrecibos \
WHERE rcdepend = %d \
AND rcorden = %d \
AND rctipo = '%s') ",
		get_curr_time(), gs_fecha, pi_depend,
		pi_depend, pi_orden, ps_tiporec );
 
    gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);

    if (gi_error_sql < 0 )
    {
        DB_check_error(gi_error_sql); 
        return; 
    }
    if (gi_error_sql == 0) 
    {
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        get_msg(30098); /* Relacion duplicada */
        return;
    }
    return;
}

/************************************************************************
*                       PR_RcActOc                                   * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Actualiza la tabla de TCMORDENCO                       *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 13-Feb-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static void
PR_RcActOc(DEPEND pi_depend, CONSEC pi_orden, TIPOREC ps_tiporec[TIPOREC_L])
{

    /* Si la OCompra es parcial pasa a Z (Recibida total)*/

    sprintf(gc_stmsql,"SELECT time_stmp FROM tcmordenco \
WHERE ocdepend IN (50,60,55, %d)  \
AND ('IMP' = '%s' OR ocorden  = occonsol) \
AND ocorden = ( SELECT rcordenc FROM trcrecibos \
WHERE rcdepend = %d \
AND rcorden = %d \
AND rctipo = '%s') ",
		pi_depend,ps_tiporec,pi_depend, pi_orden, ps_tiporec );
 
    gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);

    if (gi_error_sql < 0 ) {
        DB_check_error(gi_error_sql); 
        return; 
    }
    else if (gi_error_sql > 0 ) {
        return;    /* Salir por no ser parcial  */
    }
	 
    sprintf(gc_stmsql,"UPDATE tcmordenco SET ocestado = 'Z', \
time_stmp ='%s', ocfecedo = '%s' \
WHERE ocdepend IN (50,60,55, %d)  \
AND ocestado = 'X' \
AND ocorden = ( SELECT rcordenc FROM trcrecibos \
WHERE rcdepend = %d \
AND rcorden = %d \
AND rctipo = '%s') ",
		get_curr_time(), gs_fecha, pi_depend,
		pi_depend, pi_orden, ps_tiporec );
 
    gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);

    if (gi_error_sql < 0 )
    {
        DB_check_error(gi_error_sql); 
        return; 
    }
    if (gi_error_sql == 0) 
    {
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        get_msg(30098); /* Relacion duplicada */
        return;
    }
    return;
}


/************************************************************************
*                       PR_RcActEstRec                                  * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Actualiza la tabla de TRCRECIBOS                       *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 13-Feb-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static void
PR_RcActEstRec(DEPEND pi_depend, CONSEC pi_orden, 
                TIPOREC ps_tiporec[TIPOREC_L], VALAL pd_valvta,
                ARTICULO pi_ultplu, CONSEC pi_conmer)
{
    TIME    ls_tiempo[TIME_L];

    /****************************************************************
    Se actualiza el estado del recibo con la finalidad de bloquear
    el envio de la aprobacion mas de una vez para el mismo recibo.
    Se deja en estado "En Proceso de Aprobacion" (Z)
    *****************************************************************/
    strcpy(ls_tiempo, get_curr_time());

    sprintf (gc_stmsql, "UPDATE trcrecibos \
SET (rcestado,rcvalvta,rcultplu,rcconmer,time_stmp) \
 = ('Z',  %.2f, %d, %d, '%s') \
WHERE rcdepend = %d AND rcorden = %d \
AND rctipo = '%s' AND time_stmp = '%s'",
		pd_valvta, pi_ultplu, pi_conmer, ls_tiempo,
		pi_depend, pi_orden,ps_tiporec, gs_tiempo );

    gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

    if (gi_error_sql < 0 ) {
        DB_check_error( gi_error_sql);
        return;
    }
    else if (gi_error_sql == 0) {
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        return;
    }
    strcpy(gs_tiempo,ls_tiempo);
}

/************************************************************************
*                       PR_RcActTotVal                                  * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Actualiza la tabla de TRCRECIBOS                       *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 13-Feb-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static void
PR_RcActTotVal(DEPEND pi_depend, CONSEC pi_orden, 
               TIPOREC ps_tiporec[TIPOREC_L], 
			   VALAL *pd_totrec, VALAL *pd_valvta)
{
    TIME gs_time[TIME_L];
    HORA ls_hrasal[HORA_L];
    VALAL gd_valvta, gd_totrec, gd_margen, gd_totfle, gd_iva;
    
    gd_totrec = FN_RcTotRec(pi_depend, ps_tiporec, pi_orden);

    gd_totfle = gd_iva = 0.0;

	/* Proveedores afectos a IVA ***/
	if (strcmp(gs_nitcc,"90310000     10") != 0) {
    	gd_iva = FN_RcIva();
    	if (gd_iva < 0) {
        	return;
    	}
    	gd_totrec+= (gd_totfle* (1 + gd_iva));
	}


    /* para obtener valvta */
    sprintf (gc_stmsql, "SELECT SUM((rdcanrec + rdcanbon) * ieprecven) \
FROM trcrecdet, tinvenest \
WHERE rdplu = ieplu \
AND rddepend = iedepend \
AND rddepend = %d \
AND rdorden = %d \
AND rdtipo = '%s'",
		pi_depend, pi_orden, ps_tiporec);

    gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

    if (gi_error_sql < 0 )
    {
        DB_check_error( gi_error_sql);
        return;
    }
    else
    if (gi_error_sql == 0)
    {
        get_msg(30324);
        set_ret_val(NOT_FOUND);
        return;
    }
    DB_dbl_get_field(1,&gd_valvta);
  
    strcpy(gs_time, get_curr_time());

    sprintf(ls_hrasal,"%c%c%c%c",
            gs_time[6],gs_time[7],gs_time[8],gs_time[9]);

    /* calculo del margen */
    if (gd_valvta == 0)
        gd_margen = 0;
    else
    if (gd_valvta >= gd_totrec)
    {
        gd_margen = ((gd_valvta - gd_totrec) / gd_valvta) * 100;
        if (gd_margen == 100)
            gd_margen = 99.99;
    }   
    else
    {
        gd_margen = ((gd_totrec - gd_valvta) / gd_totrec) * -100;
        if (gd_margen == -100)
            gd_margen = -99.99;
    }

    /*Actualizacion de rcvalvta y rctotrec */
    sprintf (gc_stmsql, "UPDATE trcrecibos \
SET rctotrec = %.3f , rcvalvta = %.3f, \
rcmargen = %.2f , rchrsalida = '%s' \
WHERE rcdepend = %d \
AND rcorden = %d \
AND rctipo = '%s'",
		gd_totrec, gd_valvta, gd_margen, ls_hrasal,
		pi_depend, pi_orden, ps_tiporec);

    gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

    if (gi_error_sql < 0 ) {
        DB_check_error( gi_error_sql);
        return;
    }
    else if (gi_error_sql == 0) {
        set_ret_val(NOT_FOUND);
        return;
    }

    *pd_totrec = gd_totrec;
    *pd_valvta = gd_valvta;

    return;
}


/************************************************************************
*                       PR_RcActRec                                  * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Actualiza la tabla de TRCRECIBOS                       *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 13-Feb-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static void
PR_RcActRec(DEPEND pi_depend, CONSEC pi_orden, TIPOREC ps_tiporec[TIPOREC_L],
            CONSEC pi_nroent )
{
    char    lc_auxi;
    int     li_plazo;
    int     li_funcion;
    int     li_diaspp;
    int     li_condi;
    FECHA   ls_fecha[FECHA_L], ls_fecha_oc[FECHA_L];
    FECHA   ls_fecha_pag[FECHA_L];
    int     li_nrooc;
    int     li_nroocp;
    char    lc_parcial;          /* Si tiene parcialidades */
    char    lc_ultima = 'U';     /* Si es la ultima parcial */
    char    lc_estado = 'T';     /* Estado orden de compra */
    double  ld_cant01;
    double  ld_cant02;
    double  ld_cant03;
    double  ld_cant04;
    char    ls_occompra[50];

    strcpy( ls_fecha_pag, gs_fecharealperiodo );
	li_diaspp = 0;

	sprintf(gc_stmsql,"TIPO RECEPCION: %s", ps_tiporec);
	WriteDebugMsg(gc_stmsql);
	
    if (strcmp( ps_tiporec, "OPL") == 0) 
       strcpy( ps_tiporec, "IMP") ;
    else if (strcmp( ps_tiporec, "RT") != 0) 
    {
    	if (strcmp( ps_tiporec, "RF") != 0) 
        sprintf (gc_stmsql, "SELECT rcplazo, rcfecha, rcfecpago, occondi, \
rcordenc, occonsol, occompra, ppplazo1, ocfliminf \
FROM trcrecibos,tcmordenco,OUTER tgnplazop \
WHERE rcorden = %d \
AND rcdepend = %d \
AND rctipo = '%s' \
AND ocorden = rcordenc \
AND ocdepend IN (50,60,55, %d) \
AND ppcodigo = rcplazo",
			pi_orden, pi_depend, ps_tiporec, pi_depend );
		else
		/**JML!  OJO con el nro. de condicion **/
        sprintf (gc_stmsql, "SELECT rcplazo, rcfecha, rcfecpago, 1, \
rcordenc, rcordenc, ' ', ppplazo1,rcfecha \
FROM trcrecibos, tinordfab, OUTER tgnplazop \
WHERE rcorden = %d \
AND rcdepend = %d \
AND rctipo = '%s' \
AND ofconsecu = rcordenc \
AND ofdepend = %d \
AND ppcodigo = rcplazo",
			pi_orden, pi_depend, ps_tiporec, pi_depend );

        gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

        if (gi_error_sql < 0 ) {
            DB_check_error( gi_error_sql);
            return;
        }
        else if (gi_error_sql == 0) {
            abort_trans(RECDB);
            print_line(rep1);
            at_col(rep1,5,"Error, No existe plazo de pago");
            print_line(rep1);
            set_ret_val (gi_error_sql);
            return;
        }
        DB_int_get_field( 1, &li_plazo );
        DB_str_get_field( 2,  ls_fecha );
        DB_str_get_field( 3,  ls_fecha_pag );
        DB_int_get_field( 4, &li_condi );
        DB_int_get_field( 5, &li_nrooc);
        DB_int_get_field( 6, &li_nroocp);
        DB_str_get_field( 7, ls_occompra);
		gi_num_oc = li_nrooc;
        if (li_plazo > 0)
            DB_int_get_field( 8, &li_diaspp );
        DB_str_get_field( 9, ls_fecha_oc );

        if (strcmp(ls_fecha_oc,ls_fecha) > 0) 
            strcpy(ls_fecha,ls_fecha_oc); 

        if (strcmp( ps_tiporec, "IMP") == 0) 
            li_nroocp = li_nrooc;

        if ( strcmp(ls_fecha_pag, "") == 0 && strcmp( ps_tiporec, "RF") != 0) 
        {
			/* Se le deja a la funcion   Oct 11/2002  ***
            numfecha = date_to_num(ls_fecha);
            numfecha += li_diaspp;
            num_to_date(numfecha,ls_fecha_pag);
			**********************************************/

            /*** FCM 06/08/98 ******** Por solicitud de Pago a Proveedores **
            *****************************************************************/
            li_funcion = FN_RcFecPago(ls_fecha, li_plazo, li_diaspp,
                         li_condi, ls_fecha_pag, pi_depend);
            if ( li_funcion < 0 )
            {
                DB_check_error( gi_error_sql);
                return;
            }
            else if ( li_funcion == 0 ) {
                abort_trans(RECDB);
                print_line(rep1);
                at_col(rep1,5,"Error, Al buscar fecha de pago" );
                print_line(rep1);
                set_ret_val (0);
                return;
            }
            /****************************************************************/
        }

        if ( li_nrooc == li_nroocp )
            lc_parcial = 'N';
        else
            lc_parcial = 'S';

        if (lc_parcial == 'N')
        {
    		if (strcmp( ps_tiporec, "RF") != 0) 
#ifdef RC_ADD_B2B
            sprintf(gc_stmsql,"CONTSELECT odacmrecp, odcanped, odcbonrec, \
odcanbon \
FROM   tcmordende, tcmordenco \
WHERE  odnumord = %d \
AND    oddepend IN (50,60,55, %d) \
AND    ocorden  = %d \
AND    ocdepend IN (50,60,55, %d) \
AND    (ocparcial= 'S' OR ocparcial= 'B')",
                               li_nrooc, pi_depend, li_nrooc, pi_depend );
#else /*RC_ADD_B2B*/
            sprintf(gc_stmsql,"CONTSELECT odacmrecp, odcanped, odcbonrec, \
odcanbon \
FROM tcmordende, tcmordenco \
WHERE odnumord = %d \
AND oddepend IN (50,60,55, %d) \
AND ocorden  = %d \
AND ocdepend IN (50,60,55,%d) \
AND ocparcial= 'S'", \
                               li_nrooc, pi_depend, li_nrooc, pi_depend );
#endif /*RC_ADD_B2B*/
			else
            sprintf(gc_stmsql,"CONTSELECT fdacmrec, fdcantidad, 0.00, \
0.00 \
FROM tinordfabd, tinordfab \
WHERE fdconsecu = %d \
AND fddepend = %d \
AND ofconsecu = %d \
AND ofdepend = %d ",
                               li_nrooc, pi_depend, li_nrooc, pi_depend );

            gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);

            if (gi_error_sql < 0)
            {
                DB_check_error(gi_error_sql);
                PR_Encabe_Error();
                set_ret_val(-1);
                at_col(rep1,15,"********ERROR EN SENTENCIA SELECT*******");
                print_line(rep1);
                return;
            }

            while (gi_error_sql > 0 && lc_estado != 'P')
            {
                DB_dbl_get_field( 1, &ld_cant01);
                DB_dbl_get_field( 2, &ld_cant02);
                DB_dbl_get_field( 3, &ld_cant03);
                DB_dbl_get_field( 4, &ld_cant04);
                if (ld_cant01 < ld_cant02 || ld_cant03 < ld_cant04)
                    lc_estado = 'P';
        
                gi_error_sql = DB_exec_sql( RECDB, NULL );
            }
        }
        else
        {
            /* Como es parcial averiguo si es la ultima */

            sprintf(gc_stmsql,"SELECT 'U'  FROM   tcmordenco \
WHERE  ocdepend IN (50,60,55, %d) \
AND    ocorden = ( SELECT MAX( ocorden) \
FROM tcmordenco \
WHERE occonsol = %d \
AND   ocdepend IN (50,60,55, %d) ) \
AND    ocorden = %d \
UNION \
SELECT 'N' \
FROM tcmordenco \
WHERE ocdepend IN (50,60,55,%d) \
AND  ocorden < ( SELECT MAX( ocorden) \
FROM tcmordenco \
WHERE occonsol = %d \
AND ocdepend IN (50,60,55,%d) ) \
AND ocorden = %d ",
                       pi_depend, li_nroocp, pi_depend, li_nrooc,
                       pi_depend, li_nroocp, pi_depend, li_nrooc );

            gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);

            if (gi_error_sql < 0) {
                DB_check_error(gi_error_sql);
                PR_Encabe_Error();
                set_ret_val(-1);
                at_col(rep1,15,"********ERROR EN SENTENCIA SELECT*******");
                print_line(rep1);
                return;
            }
            else if (gi_error_sql == 0) {
                abort_trans(RECDB);
                set_ret_val(-1);
                PR_Encabe_Error();
                at_col(rep1,15,"***** NO ENCUENTRA ORDEN DE COMPRA *****");
                print_line(rep1);
                return;
            }
            DB_char_get_field(1, &lc_ultima);

            if ( lc_ultima == 'N' )  /* Si no es la ultima parcial, pasa a P */
                lc_estado = 'P';
        }
    	if (strcmp( ps_tiporec, "RF") != 0) 
        	sprintf(gc_stmsql,"UPDATE tcmordenco \
SET ocestado = '%c', \
time_stmp = '%s' \
WHERE ocorden  = %d \
AND ocdepend IN (50,60,55,%d) ",
                           lc_estado, get_curr_time(),
                           li_nroocp, gi_depend );
		else
        	sprintf(gc_stmsql,"UPDATE tinordfab \
SET ofestado = '%c', \
time_stmp = '%s' \
WHERE ofconsecu  = %d \
AND ofdepend = %d ",
                           lc_estado, get_curr_time(),
                           li_nroocp, gi_depend );


        gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);

        if (gi_error_sql < 0) {
            DB_check_error(gi_error_sql);
            PR_Encabe_Error();
            set_ret_val(-1);
            at_col(rep1,15,"******* ERROR EN SENTENCIA UPDATE ******");
            print_line(rep1);
            return;
        }
        else if (gi_error_sql == 0) {
            abort_trans(RECDB);
            PR_Encabe_Error();
            set_ret_val(-1);
            at_col(rep1,15,"******** NO ENCUENTRA ORDEN DE COMPRA *******");
            print_line(rep1);
            return;
        }
    }
    /********************************************************************
    Se actualiza el valor de venta del recibo y el estado a Aprobado.     
    NOTA: Se actualiza el time_stmp, pero no se controla ya que se hizo
    previamente.
    **********************************************************************/
    
    sprintf (gc_stmsql, "UPDATE trcrecibos \
SET rcestado = 'A', rccompen = %.3f, \
rcnroent = %d,  rcestado2 = 'T', \
rcfecpago = '%s', rcfecpagap = '%s',time_stmp = '%s' \
WHERE rcdepend = %d AND rcorden = %d \
AND rctipo = '%s' AND rcestado MATCHES '[IZ]'",
                         gd_compen, pi_nroent,  ls_fecha_pag, GetCurrDate(0), 
                         get_curr_time(),
                         pi_depend, pi_orden, ps_tiporec);

    gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

    if (gi_error_sql < 0 )
    {
        DB_check_error( gi_error_sql);
        return;
    }
    else
    if (gi_error_sql == 0)
    {
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        return;
    }

	/* Borra todos los registros que quedan en 0 * FCM 06/08/98 ****/
    sprintf (gc_stmsql, "DELETE FROM trcrecdet \
WHERE rddepend = %d \
AND rdorden = %d \
AND rdtipo = '%s' \
AND rdcanrec = 0 AND rdcanbon = 0",
                         pi_depend, pi_orden, ps_tiporec);

    gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);
    if (gi_error_sql < 0 ) {
        DB_check_error( gi_error_sql);
        return;
    }

	/* Si no se registro ningun item, se aborta el proceso FCM 04/04/02 ****/
    sprintf (gc_stmsql, "SELECT rdplu FROM trcrecdet \
WHERE rddepend = %d \
AND rdorden = %d \
AND rdtipo = '%s'",
                         pi_depend, pi_orden, ps_tiporec);

    gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);
    if (gi_error_sql < 0 ) {
        DB_check_error( gi_error_sql);
        return;
    }
    else if (gi_error_sql == 0 ) {
            abort_trans(RECDB);
            PR_Encabe_Error();
            set_ret_val(-1);
            at_col(rep1,15,"******** NO SE DIGITO LA RECEPCION *******");
            print_line(rep1);
            return;
	}

    gi_error_sql =1;
    return;
}

/************************************************************************
*                       PR_RcActDes                                  * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Actualiza la tabla de TDBDESPA                       *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 13-Feb-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static void
PR_RcActDes(DEPEND pi_depend, DEPEND pi_depdes, CONSEC pi_orden,
            TIPOREC ps_tiporec[TIPOREC_L])
{

    sprintf (gc_stmsql, "UPDATE tdbdespa \
SET deestado = 'R', time_stmp = '%s' \
WHERE dedepend = %d AND dedepsol = %d \
AND denumguia = ( SELECT rcnumguia FROM trcrecibos \
WHERE rcdepend = %d \
AND rcorden = %d AND rctipo = '%s')",
                         get_curr_time(),pi_depdes,pi_depend,
                         pi_depend, pi_orden, ps_tiporec);

    gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);
    
    if (gi_error_sql < 0 )
    {
        DB_check_error( gi_error_sql);
        return;
    }
    else
    if (gi_error_sql == 0)
    {
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        return;
    }

    return;
}

/************************************************************************
*                       PR_RcActFabDet                                  * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Actualiza el detalle de la orden de compra.              *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 02-Feb-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static void
PR_RcActFabDet(ARTICULO pi_plu, CANTINV pd_canrec, CANTINV pd_canbon,
            CANTINV pd_candev, int pi_ordcompp, char pc_cant)
{
    CANTINV ld_cantid1, ld_cantid2, ld_canrec1, ld_canbon1;
    FACTUNI ld_factor;

    gc_parcial = ' ';
    gi_ordcomp = pi_ordcompp;

    gd_canpedida=0.0;

	/**JML!  poner cantidades acumuladas **/
    sprintf (gc_stmsql,"SELECT fdcantidad - fdacmrec \
FROM tinordfabd \
WHERE fddepend = %d AND fdconsecu = %d \
AND fdplures = %d ",
		gi_depend, gi_ordcomp, pi_plu);

    gi_error_sql = DB_exec_sql (CMPDB, gc_stmsql);

    if (gi_error_sql < 0 )
    {
        DB_check_error( gi_error_sql);
        return;
    }
    if (gi_error_sql == 0)
    {  
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        return;
    }
    if ( dbl_get_field( 1,&ld_cantid1) < 0 ) return;/*Pedida */
    ld_cantid2 = 0; /* Bonificada pedida */
    ld_factor = 1;

    gd_canpedida+= ld_cantid1+ld_cantid2;
    if (ld_cantid1 > 0 || ld_cantid2 > 0)
    {
        ld_canrec1 = pd_canrec;
        ld_canbon1 = pd_canbon;
        if (ld_cantid1 < pd_canrec)
            ld_canrec1 = ld_cantid1;
        /****************************************************************
        Acumular presupuesto:
        - El indicador de recibos parciales = N
        1. Se valoriza la cantidad PENDIENTE POR RECIBIR a costo sin
           impuesto.
        ******************************************************************/
        if (gc_parcial == 'N')
			/* No aplicar factor JML!2000/03/14 
            gd_vrpresup +=  gprrecdet->rdcosto * 
                                ((ld_cantid1 - ld_canrec1)/ld_factor);
    		******************/
            gd_vrpresup +=  gprrecdet->rdcosto * (ld_cantid1 - ld_canrec1);

    }

    if (pd_canrec > 0) {	
    	/*Actualiz de detalle de unica o la padre de entregas parciales **/
    	sprintf (gc_stmsql, "UPDATE tinordfabd \
SET (fdacmrec, time_stmp) = \
(fdacmrec + %.3f, '%s') \
WHERE fddepend = %d AND fdconsecu = %d \
AND fdplures = %d",
                         pd_canrec, 
                         get_curr_time(),gi_depend,gi_ordcomp, pi_plu);
    
    	gi_error_sql = DB_exec_sql (CMPDB, gc_stmsql);

    	if (gi_error_sql < 0 ) {
        	DB_check_error( gi_error_sql);
        	return;
    	}
    	if (gi_error_sql == 0) {
        	abort_trans(RECDB);
        	set_ret_val(NOT_FOUND);
        	return;
    	}

	    /** Actualizar Cantidades Stock Proveedor ***************/
    	sprintf (gc_stmsql,"UPDATE tinfabrica \
SET fbexisten = fbexisten - \
(SELECT SUM(ficantins * %.2f) FROM tinordfadd \
WHERE ficonsecu = %d \
AND   fidepend  = %d \
AND   fipluins  = fbplu \
AND   fiplures  = %d ), \
fbenproce = fbenproce - \
(SELECT SUM(ficantins * %.2f) FROM tinordfadd \
WHERE ficonsecu = %d \
AND   fidepend  = %d \
AND   fipluins  = fbplu \
AND   fiplures  = %d ), \
time_stmp = '%s' \
WHERE fbnitcc = '%s' \
AND   fbplu IN \
(SELECT fipluins FROM tinordfadd \
WHERE ficonsecu = %d \
AND   fidepend  = %d \
AND   fiplures  = %d ) ",
        pd_canrec, gi_ordcomp, gi_depend, pi_plu,
        pd_canrec, gi_ordcomp, gi_depend, pi_plu,
        get_curr_time(),
		gs_nitcc,
		gi_ordcomp, gi_depend, pi_plu);
    
    	gi_error_sql = DB_exec_sql (CMPDB, gc_stmsql);

    	if (gi_error_sql < 0 ) {
        	DB_check_error( gi_error_sql);
        	return;
    	}
    	if (gi_error_sql == 0) {
		/** JML!
        	abort_trans(RECDB);
        	set_ret_val(NOT_FOUND);
		gi_error_sql = 1;
		******/
        	abort_trans(RECDB);
        	set_ret_val(NOT_FOUND);
        	at_col(rep1,5,"No se encontro plus insumos en existencia del proveedor"); 
        	print_line(rep1);
        	return;
    	}
	}
}

/************************************************************************
*                       PR_RcActOcDet                                   * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Actualiza el detalle de la orden de compra.              *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 02-Feb-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static void
PR_RcActOcDet(ARTICULO pi_plu, CANTINV pd_canrec, CANTINV pd_canbon,
            CANTINV pd_candev, int pi_ordcompp, char pc_cant)
{
    CANTINV ld_cantid1, ld_cantid2, ld_canrec1, ld_canbon1;
    FACTUNI ld_factor;
    char    lc_swapvta;
    char    lc_estado;
    double  ld_precvta;
    int 	li_bodega;
	int     gi_ordcomp; 

	// Valida si es compra importada y si el indicado pi_ordcompp == 0 actualiza datos de la OC importada.
	if ( strcmp(gs_tiporec, "IMP") == 0 && pi_ordcompp == 0)
	{
		/* Actuliza detalle */
		sprintf (gc_stmsql, "UPDATE tcmordende SET (odacmrecp, odcbonrec, odacmrech,time_stmp) = \
(odacmrecp + %.3f, odcbonrec + %.3f, odacmrech + %.3f, '%s') \
WHERE oddepend = %d AND odplu = %d \
AND odnumord IN (select rcordenc from trcrecibos where rcdepend=%d and rcorden=%d and rctipo='%s')", 
		pd_canrec, pd_canbon, pd_candev, get_curr_time(), gi_depend, pi_plu,
		gi_depend, gi_orden, gs_tiporec);
		
		gi_error_sql = DB_exec_sql (CMPDB, gc_stmsql);

		if (gi_error_sql < 0 ) {
			DB_check_error( gi_error_sql);
			return;
		}
		if (gi_error_sql == 0) {
			abort_trans(RECDB);
			set_ret_val(NOT_FOUND);
			return;
		}	
	
        // if (update_embarques(gi_depend, gi_orden, pi_plu, pd_canrec) < 0) {
            // sprintf(gc_stmsql, "Error al actualizar embarques de OC %d", gi_orden);
            // WriteDebugMsg(gc_stmsql);
            // DB_check_error( gi_error_sql);
            // return;
        // }
    }
	
    gc_parcial = ' ';
    if (pi_ordcompp > 0)
        sprintf (gc_stmsql, "SELECT ocorden, ocparcial, ocestado ,rcdepend \
FROM tcmordenco, trcrecibos \
WHERE ocdepend IN (50,60,55, %d) \
AND ocorden = rcordenc \
AND rcdepend = %d \
AND rcorden = %d AND rctipo = '%s' ",
                         gi_depend, gi_depend, gi_orden, gs_tiporec);
    else
        sprintf (gc_stmsql, "SELECT ocorden, ocparcial, ocestado,ocdepend \
FROM tcmordenco, tcmordende, tgnnitster \
WHERE ocdepend IN (60,50,53,55) \
AND ocestado IN ('A','P','I') \
AND ntorigen = 'E' \
AND ocnitcc = ntnitcc \
AND odplu = %d \
AND oddepend = ocdepend AND odnumord = ocorden",
                              pi_plu);


    gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

    if (gi_error_sql < 0 ) {
        DB_check_error( gi_error_sql);
        return;
    }
    if (gi_error_sql == 0) {
        if (pi_ordcompp > 0) {
           abort_trans(RECDB);
           set_ret_val(NOT_FOUND);
           return;
        }
        else {
           gi_error_sql = 1;
           return;
        }
    }

    if ( DB_int_get_field(  1, &gi_ordcomp) < 0 ) return;
    if ( DB_char_get_field( 2, &gc_parcial) < 0 ) return;
    if ( DB_char_get_field( 3, &lc_estado) < 0 ) return;
    if ( DB_int_get_field( 4, &li_bodega) < 0 ) return;
     
	if (li_bodega == 50 && gi_depend == 60) li_bodega = 60;
    gd_canpedida=0.0;

    sprintf (gc_stmsql,"SELECT odcanped - odacmrecp,\
odcanbon - odcbonrec, unfactor, \
odswapvta, odprecvta \
FROM tcmordende, tgnunidads \
WHERE oddepend IN (50,60,55,%d) AND odnumord = %d \
AND odplu = %d AND odundemp = ununidad ",
                        li_bodega, gi_ordcomp, pi_plu); 

    gi_error_sql = DB_exec_sql (CMPDB, gc_stmsql);

    if (gi_error_sql < 0 ) {
        DB_check_error( gi_error_sql);
        return;
    }
    if (gi_error_sql == 0) {  
        if (pi_ordcompp == 0) {
           gi_error_sql = 1; 
           return;
        }
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        return;
    }

    if ( DB_dbl_get_field(  1, &ld_cantid1 ) < 0 ) return;/*Pedida */
    if ( DB_dbl_get_field(  2, &ld_cantid2 ) < 0 ) return;/*Bonificada pedida */
    if ( DB_dbl_get_field(  3, &ld_factor  ) < 0 ) return;
    if ( DB_char_get_field( 4, &lc_swapvta ) < 0 ) return;
    if ( DB_dbl_get_field(  5, &ld_precvta ) < 0 ) return;  


    gd_canpedida+= ld_cantid1+ld_cantid2;
    if (ld_cantid1 > 0 || ld_cantid2 > 0) {
        ld_canrec1 = pd_canrec;
        ld_canbon1 = pd_canbon;
        if (ld_cantid1 < pd_canrec)
            ld_canrec1 = ld_cantid1;
        /****************************************************************
        Acumular presupuesto:
        - El indicador de recibos parciales = N
        1. Se valoriza la cantidad PENDIENTE POR RECIBIR a costo sin
           impuesto.
        ******************************************************************/
        if (gc_parcial == 'N')
			/* No aplicar factor JML!2000/03/14 
            gd_vrpresup +=  gprrecdet->rdcosto * 
                                ((ld_cantid1 - ld_canrec1)/ld_factor);
    		******************/
            gd_vrpresup +=  gprrecdet->rdcosto * (ld_cantid1 - ld_canrec1);

    }

    /*Actuliz de detalle de unica o la padre de entregas parciales */
    sprintf (gc_stmsql, "UPDATE tcmordende \
SET (odacmrecp, odcbonrec, odacmrech,time_stmp) = \
(odacmrecp + %.3f, odcbonrec + %.3f, \
odacmrech + %.3f, '%s') \
WHERE oddepend IN (50,60,55, %d) AND odnumord = %d \
AND odplu = %d",
                         pd_canrec, pd_canbon, pd_candev,
                         get_curr_time(),li_bodega,gi_ordcomp, pi_plu);
    
    gi_error_sql = DB_exec_sql (CMPDB, gc_stmsql);

    if (gi_error_sql < 0 ) {
        DB_check_error( gi_error_sql);
        return;
    }
    if (gi_error_sql == 0) {
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        return;
    }

    if ( pc_cant == 'M' ) /* Hijas de entraga parciales */
    {
        sprintf (gc_stmsql, "UPDATE tcmordende \
SET (odacmrecp,odcbonrec,odacmrech,time_stmp)=\
(odacmrecp + %.3f, odcbonrec + %.3f, \
odacmrech + %.3f, '%s') \
WHERE oddepend IN (50,55,%d) AND odnumord = %d \
AND odplu = %d",
                             pd_canrec, pd_canbon, pd_candev, 
                             get_curr_time(),li_bodega,pi_ordcompp,pi_plu);
  
        gi_error_sql = DB_exec_sql (CMPDB, gc_stmsql);

        if (gi_error_sql < 0 ) {
            DB_check_error( gi_error_sql);
            return;
        }
        else if (gi_error_sql == 0) {
            abort_trans(RECDB);
            set_ret_val(NOT_FOUND);
            return;
        }
    }

    if ( pi_ordcompp == 0 && lc_estado == 'I' ) {
        sprintf (gc_stmsql, "UPDATE tcmordenco \
SET ocestado = 'P', occompra = 'ORION', \
time_stmp = '%s' \
WHERE ocdepend IN (50,60,55,%d) AND ocorden = %d ",
			get_curr_time(),li_bodega,gi_ordcomp);
  
        gi_error_sql = DB_exec_sql (CMPDB, gc_stmsql);

        if (gi_error_sql < 0 ) {
            DB_check_error( gi_error_sql);
            return;
        }
        else if (gi_error_sql == 0) {
            abort_trans(RECDB);
            set_ret_val(NOT_FOUND);
            return;
        }
    }
    if ( lc_swapvta == 'R' ) {
        gi_error_sql = FN_GpCamPrecVta(pi_plu, ld_precvta, gi_depend);
    
        if (gi_error_sql < 0 ) {
            DB_check_error( gi_error_sql);
            return;
        }
        else if (gi_error_sql == 0) {
            abort_trans(RECDB);
            set_ret_val(NOT_FOUND);
            return;
        }
    }  
}

/************************************************************************
*                       PR_RcActDesdet                                  * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Actualiza el detalle de la Orden de Compra               *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 02-Feb-95    creacion                                 FAM             *
* NOTA: Se considera la actualizacion de la bodega                      *
* Se usa el GATEWAY
*                                                                       *
*************************************************************************/
static void
PR_RcActDesdet(ARTICULO pi_plu, CANTINV pd_canrec) 
{
    CANTINV ld_cantid1, ld_canrec1, ld_cantdesp;
    int li_fin, li_in;
    SWITCH lc_estdes;

    sprintf (gc_stmsql,"SELECT ddcandes, ddcanrec, ddordend \
FROM tdbdesdet,trcrecibos \
WHERE  dddepend = %d AND dddepsol = %d \
AND ddorden = rcordenc AND ddplu = %d \
AND rcdepend = %d AND rcorden = %d \
AND rctipo = '%s'",
                        gi_depdes,gi_depend, pi_plu,
                        gi_depend, gi_orden, gs_tiporec);

    gi_error_sql = DB_exec_sql (DISDB, gc_stmsql);

    if (gi_error_sql < 0 )
    {
        DB_check_error( gi_error_sql);
        return;
    }
    if (gi_error_sql == 0)
    {
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        return;
    }
    if ( dbl_get_field( 1,&ld_cantdesp) < 0 ) return;/*Recibida */
    if ( dbl_get_field( 2,&ld_cantid1) < 0 ) return;/*Recibida */
    if ( int_get_field( 3,&gi_nserie) < 0 ) return;/*Recibida */

    gd_cantdesp = ld_cantdesp;

    /* Si la cantidad recibida corresponde a la despachada */
    if (ld_cantdesp != pd_canrec)
        lc_estdes = 'S';
    else
        lc_estdes = 'N';

    sprintf (gc_stmsql, "UPDATE tdbdesdet \
SET (ddcanrec, ddestado, time_stmp) = \
(ddcanrec + %.3f, '%c', '%s') \
WHERE dddepend = %d AND dddepsol = %d \
AND ddplu = %d \
AND ddorden = (SELECT rcordenc \
FROM trcrecibos WHERE \
rcdepend = %d AND rcorden = %d \
AND rctipo = '%s' )",
                        pd_canrec, lc_estdes, get_curr_time(), gi_depdes, 
                        gi_depend, pi_plu,gi_depend,gi_orden, gs_tiporec);

    gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

    if (gi_error_sql < 0 )
    {
        DB_check_error( gi_error_sql);
        return;
    }
    if (gi_error_sql == 0)
    {
        abort_trans(RECDB);
        set_ret_val(NOT_FOUND);
        return;
    }

    return;
}

static int
FN_CmActPptoL(int p_nivagr, int p_codniv, 
	     char p_tipo, char p_fecord[FECHA_L], 
	     char p_fecha[FECHA_L], NIT p_nitcc[NIT_L])

{
	
	struct _seccion {
		int 	codigo;
		char	descrip[50];
		double	valor;
		SWITCH	tipo;
	} seccion[1000];
	int li_numsec,i;
	int li_signo = 1;
	
    if (p_tipo == 'E') { 
		li_signo = -1;
		/* Evalua el presupuesto para cada una de las secciones */
		sprintf(gc_stmsql,"MULTISELECT(1000) panivel02, nadescrip, \
sum((odcanped-odacmrecp)*odctoint), (SELECT octipo FROM tcmordenco WHERE ocorden = odnumord) \
FROM tcmordende, tmpnaartplu, tmpnivagr \
WHERE oddepend IN (50,55,%d) AND odnumord = %d \
AND paplu = odplu \
AND naclave = panivel02+20000 \
GROUP BY 1,2,4",
				gi_depend, gi_ordcomph);
    }
	else {
		/* Evalua el presupuesto para cada una de las secciones */
		sprintf(gc_stmsql,"MULTISELECT(1000) panivel02, nadescrip, \
SUM(rdcanrec*rdcosto), (SELECT octipo \
FROM tcmordenco, trcrecibos \
WHERE rcorden = rdorden AND rctipo = rdtipo AND rcdepend = rddepend AND ocorden = rcordenc) \
FROM trcrecdet, tmpnaartplu, tmpnivagr \
WHERE rddepend = %d AND rdorden = %d \
AND paplu = rdplu \
AND naclave = panivel02+20000 \
GROUP BY 1,2,4", 
				gi_depend, gi_orden);
   }
	
	gi_error_sql=DB_exec_sql(CMPDB,gc_stmsql);
	if (gi_error_sql<=0)
		return -1;
	
	li_numsec = gi_error_sql;
	for (i=0; i < li_numsec; i++)
	{
	   DB_int_get_field(i*4+1, &seccion[i].codigo);
	   DB_str_get_field(i*4+2, seccion[i].descrip);
	   DB_dbl_get_field(i*4+3, &seccion[i].valor);
	   if (DB_char_get_field(i*4+4, &seccion[i].tipo) < 0) seccion[i].tipo = 'N';
	}
	
	WriteDebugMsg("SE CAYO4");
	
	for (i=0; i < li_numsec; i++)
	{
		if (seccion[i].tipo == 'S')
			break;
		if ( FN_CmActPpto( 2, seccion[i].codigo, p_tipo, 
					p_fecord, p_fecha,
					seccion[i].valor*li_signo, p_nitcc ) <= 0 ) {
			at_col(rep1,5,"Error : al actualizar Presupuesto");
			print_line(rep1);
			abort_trans (RECDB);
			set_ret_val (-1);
			return(0);
		}
	}
	return(1);
}



/************************************************************************
*                       PR_RcSacaPila                                   * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Lee un registro de recibos - detalle                     *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 08-May-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static void
PR_RcSacaPila()
{
    if ( int_get_field( 1,&(gprrecdet->rdplu)) < 0 ) return;
    if ( dbl_get_field( 2,&(gprrecdet->rdcanrec)) < 0 ) return;
    if ( dbl_get_field( 3,&(gprrecdet->rdcanbon)) < 0 ) return;
    if ( dbl_get_field( 4,&(gprrecdet->rdcandev)) < 0 ) return;
    if ( dbl_get_field( 5,&(gprrecdet->rdcosto)) < 0 ) return;
    if ( dbl_get_field( 6,&(gprrecdet->rdcosrep)) < 0 ) return;
    if ( dbl_get_field( 7,&(gprrecdet->rdprelis)) < 0 ) return;
    if ( str_get_field( 8,gprrecdet->rdunidad) < 0 ) return;
    if ( str_get_field( 9,gprrecdet->rdcambun) < 0 ) return;
    if ( str_get_field( 10,gs_fecha) < 0 ) return;
    if ( str_get_field( 11,gs_horarec) < 0 ) return;
    if ( str_get_field( 12,gs_anden) < 0 ) return;
    if ( int_get_field( 13,&gi_codiva) < 0 ) return;
}

static int FN_CmObtNodo(NIVEL_A pi_nivagr,ID_NIVEL pi_codniv,char pc_resto)
{
    NIVEL_A  li_nivagr;
    ID_NIVEL li_codniv;
    VALAL    ld_costo;
    int      li_retfun;
    
    if (pi_nivagr > 1){
        sprintf(gc_stmsql,"SELECT psnivagr,pscodniv \
FROM tcmpresup,tmpnivagr,tinvalper,tgndepende \
WHERE naclave = %d AND '%s' BETWEEN vafeiper AND vafefiper \
AND declase = 'A' AND dedepend = vadepend AND vaperiodo = psperiodo \
AND ((psnivagr = 1 AND nanivel01 = pscodniv) \
OR (psnivagr = 2 AND nanivel02 = pscodniv) \
OR (psnivagr = 3 AND nanivel03 = pscodniv) \
OR (psnivagr = 4 AND nanivel04 = pscodniv))",pi_nivagr*10000+pi_codniv,
               gs_fecha);

        gi_error_sql = DB_exec_sql(GENDB,gc_stmsql);

        if (gi_error_sql < 0){
            at_col(rep1,5,"Error Base de Datos: al actualizar Presupuesto");
            print_line(rep1);
            return(-1);
        }
        
        WriteDebugMsg("SE CAYO5");
        
        if (gi_error_sql == 0 && pi_nivagr == 5){
            at_col(rep1,5,"Error : al actualizar Presupuesto");
            print_line(rep1);
            abort_trans (RECDB);
            set_ret_val (-1);
            return(0);
        }
        if (gi_error_sql > 0){
            DB_int_get_field(1,&li_nivagr);
            DB_int_get_field(2,&li_codniv);

            li_retfun = FN_CmActPptoL(li_nivagr, li_codniv, 'R', gs_fecord, 
                                     gs_fecha,  gs_nitcc);
            if (li_retfun < 0)
            {
                at_col(rep1,5,"Error Base de Datos: al actualizar Presupuesto");
                print_line(rep1);
                return(-1);
            }
            if (li_retfun == 0)
            {
                at_col(rep1,5,"Error : al actualizar Presupuesto");
                print_line(rep1);
                abort_trans (RECDB);
                set_ret_val (-1);
                return(0);
            }
            return(1); 
        }
    }

    sprintf(gc_stmsql,"CONTSELECT psnivagr,pscodniv \
FROM tmpnivagr,tcmpresup,tgndepende,tinvalper \
WHERE nanivel0%d = %d AND declase = 'A' AND vadepend = dedepend \
AND vaestado = 'A' AND psperiodo = vaperiodo \
AND naclave = psnivagr * 10000 + pscodniv",pi_nivagr, pi_codniv);

    gi_error_sql = DB_exec_sql(GENDB,gc_stmsql);

    if (gi_error_sql < 0){
        at_col(rep1,5,"Error Base de Datos: al actualizar Presupuesto");
        print_line(rep1);
        return(-1);
    }
    if (gi_error_sql == 0){
        at_col(rep1,5,"Error : al actualizar Presupuesto");
        print_line(rep1);
        abort_trans (RECDB);
        set_ret_val (-1);
        return(0);
    }

    while(gi_error_sql > 0){
        DB_int_get_field(1,&li_nivagr);
        DB_int_get_field(2,&li_codniv);

        if (pc_resto == 'N')
            sprintf(gc_stmsql,"SELECT SUM(rdcosto*rdcanrec) \
FROM trcrecdet,tmpnivagr,tmpplus \
WHERE rdorden = %d AND rddepend = %d AND rdtipo = '%s' AND nanivel0%d = %d \
AND rdplu = plplu AND naclave = plnivagre * 10000 + plcodnive",gi_orden,
                    gi_depend,gs_tiporec,li_nivagr,li_codniv);
        else
            sprintf(gc_stmsql,"SELECT SUM(odcanped-odacmrecp)*(-1) \
FROM tcmordende,trcrecibos,tmpplus,tmpnivagr \
WHERE rcordenc = %d AND rcdepend = %d AND rctipo = '%s' \
AND odnumord = rcordenc AND oddepend IN (50,60,55,%d)  AND odplu = plplu \
AND naclave = plnivagre * 10000 + plcodnive AND nanivel0%d = %d",gi_orden,
                    gi_depend,gs_tiporec,gi_depend,pi_nivagr,pi_codniv);

        gi_error_sql = DB_exec_sql(GENDB,gc_stmsql);

        if (gi_error_sql < 0){
            at_col(rep1,5,"Error Base de Datos: al actualizar Presupuesto");
            print_line(rep1);
            return(-1);
        }

        DB_dbl_get_field(1,&ld_costo);

        
        li_retfun = FN_CmActPptoL(li_nivagr, li_codniv, 'R', gs_fecord, 
                                 gs_fecha,  gs_nitcc);
        if (li_retfun < 0)
        {
            at_col(rep1,5,"Error Base de Datos: al actualizar Presupuesto");
            print_line(rep1);
            return(-1);
        }
        if (li_retfun == 0)
        {
            at_col(rep1,5,"Error : al actualizar Presupuesto");
            print_line(rep1);
            abort_trans (RECDB);
            set_ret_val (-1);
            return(0);
        }

        gi_error_sql = DB_exec_sql(GENDB,NULL);
    }
    return(1);
}

/************************************************************************
*                       PR_DesglosaIMP(char *)                          * 
*                                                                       *
* Descripcion: Desglosa contenido de la variable pasada por parametro.  *
*                                                                       *
*************************************************************************/
static void PR_DesglosaIMP(char linea[21])
{
   char *p, ls_token[10];
   int  li_token;
   
   gi_credId=0; gi_ejerc=0; gi_embcorr=0;
   li_token = 1;
   p=strtok(linea,"_");
   while(p != NULL)
   {
	  strcpy(ls_token, p);
	  ls_token[strlen(ls_token)] = '\0';
	  
	  /*sprintf(gc_stmsql,"token : %s", ls_token);
	  WriteDebugMsg(gc_stmsql);*/

	  switch (li_token) {
		  case 1:{
			  gi_credId = atoi(ls_token);				  
			  break;
		  }
		  case 2:{
			  gi_ejerc = atoi(ls_token);
			  gi_ejerc = 2000 + gi_ejerc;
			  break;
		  }              
		  case 3:{
			  gi_embcorr = atoi(ls_token);
			  break;
		  }
	  }
	  p=strtok(NULL,"_");
	  li_token++;
   }
   
   return;
}

/************************************************************************
*                       PR_Rcaprobar                                    * 
*                                                                       *
* Parametros:                                                           *
*                                                                       *
*                                                                       *
* Descripcion: Aprueba un recibo                                        *
*                                                                       *
* Algoritmo:                                                            *
*                                                                       *
* Fecha        Descripcion                              Responsable     *
* 30-Ene-95    creacion                                 FAM             *
*                                                                       *
*************************************************************************/
static int
PR_Rcaprobar()
{
    VALAL    ld_valvta, 
             ld_total, ld_totalrec;
    ARTICULO li_plu, 
             li_plhijo, 
             li_ultplu;
    int      li_funpre, 
             li_sw, 
             li_retfun, 
             li_nroent, 
             li_conmer,
             li_commit,
             li_packing=0;
    int      li_totper = 0;
    int		 li_ins, li_totinsumos;
    CANTINV  ld_transito; /* Para actualizar cantidad en transito */
    CANTINV  ld_ordena, ld_cantfab; /* Para actualizar cantidad ordenada */
    COSTINV  ld_costinsum, ld_costplures; 
    COSTINV  ld_orioncostri = 0;
    PERIODO  li_periodo;
    ESTADO	 lc_estado;
    int      li_pos, li_ind, li_numrec;
    int      li_ordcompp;     /* Orden de compra padre */
    int      li_nivapr;  
    char     mensaje[50];
    UBICA	 ls_ubica[UBICA_L];
    EAN128 ls_codcaja[EAN128_L], sPallet[EAN128_L];
    char     ls_qryrec[50];
    struct tincajarec stcajas;
    int 	 li_cajas;
    char     lc_cantordp;     /* cantidad de ordenes de compra */
                              /* U ->una ; M -> mas de una */
	SWITCH lc_octipo;
    
    struct _insumos {
		int		plu;
		double	cant;
	} ls_pluins[50];
	int li_ocplazo, li_plazo, li_fecha, li_ordenes, li_idx;
    FECHA  ls_fecpag[FECHA_L], ls_fliminf[FECHA_L];
	CONSEC li_carpeta, li_embarque, li_ejercicio, iSecuen, iTipo;

    struct _citas {
		int		modulo;
		int		unipro;
		int		modurec;
		double	cantrec;
	} reg_citas;
    char ls_upmodrec[200], ls_paramIMP[31];
    char lc_swpos;     
	int li_distribucion;
	OBSERV ls_tipoflujo[OBSERV_L];
	TIPOREC ls_tipoRecep[TIPOREC_L];
	
	// Bulto Tarea
	ARTICULO iArt;
	ATRIBUTO iColor;
	int iTareas;
	SINO cBultoTarea;

	ls_ubica[0] = '\0';
    str_get_field ( 1, gs_nomrep);
    int_get_field ( 2, &gi_depend);
    int_get_field ( 3, &gi_orden);
    str_get_field ( 4, gs_tiporec);
    str_get_field ( 5, gs_usuario);
    char_get_field ( 6, &gc_swrec);
    int_get_field(7, &li_distribucion);
	if (str_get_field(8, ls_paramIMP) < 0 ) strcpy(ls_paramIMP,"");

	char ls_msgAux[100];
	double ld_resultado;
	
	 
	sprintf(ls_msgAux,"TIPO RECEP  : %s, ParamIMP: %s",gs_tiporec, ls_paramIMP);
	WriteDebugMsg(ls_msgAux);
	
	// Agregado el 31-Dic-2015. Para filtra costo comex segun Ejerc/Embarq/Correl.
	if (strcmp(gs_tiporec,"IMP") == 0) {
	    PR_DesglosaIMP(ls_paramIMP);
		sprintf(ls_msgAux,"Valores COMEX: Cred_id=%d, Ejerc=%d, Emb_Corr=%d", gi_credId, gi_ejerc, gi_embcorr);
	    WriteDebugMsg(ls_msgAux);
	}
	
    if (FN_GetParam( &gprparamet ) <= 0)
    {
        fprintf(stderr,"RRcAprRec,No Existen Parametros \n");
        set_ret_val(-1);
        return(-1);
    }
    gi_maxlin=gprparamet->palinrep;
    gi_marlin=gprparamet->pamargrep;

    if ((rep1 = start_report(gs_nomrep,MAXCOL,gi_maxlin)) < 0)
    {
        set_ret_val (-1);
        return(-1);
    }

    if (DesDepOrigen(gs_dep) <= 0)
    {
        PR_Encabe_Error();
        at_col(rep1,15,"RRcAprRec: No se Encontro Informacion de Dependencia");
        print_line(rep1);
        set_ret_val(-1);
        return(-1);
    }

    PR_Encabe_Error();
	
	
	if (strcmp(gs_tiporec,"EM") == 0 )
	{
			if(gprparamet->papais == CHILE && GetCurrDepend() != BODEGA_INSUMOS && strcmp(gs_usuario, "LOGFIRE") != 0) 
			{
			
			  /*Validación de Aprobación de Documento*/
				sprintf(gc_stmsql, "SELECT count(*) from trcdocprov \
WHERE dcdepend = %d \
AND dcorden = %d \
AND dctipo = '%s' \
AND (dcfemision = '19000101' \
OR dcmontobruto = 0)",gi_depend,gi_orden,gs_tiporec);

				gi_error_sql = DB_exec_sql(GENDB, gc_stmsql);	

			if (gi_error_sql < 0 )
            {
               DB_check_error( gi_error_sql);
              return -1;
           }
           						
			if (DB_dbl_get_field(1, &ld_resultado) < 0) return(-1);
					
				if (ld_resultado!= 0)		
				{
					at_col(rep1,5,
					"Error: No se puede realizar APROBACION. información de documento incompleta en documento tributario.");
					print_line(rep1);
					at_col(rep1,5,
					"Complete la información en Aplicación SICA(Opción RECIBOS/INGRESO DE DOCUMENTOS).");
					print_line(rep1);
					return(-1);
                } 
			
			}
			
			
	}
	
	 
	

    /* Se obtiene el periodo corriente para futuras actualizaciones */
    li_periodo = FN_InPerActual (gi_depend, &gprvalper);
    if (li_periodo < 0)
    {
        sprintf(dummy_str,"Error en la Base de Datos %d",gi_error_sql);
        at_col(rep1,5,dummy_str);
        print_line(rep1);
        at_col(rep1,5,"Al tratar de obtener periodo activo, dependencia");
        print_line(rep1);
        set_ret_val(gi_error_sql);
        return(-1);
    }
    else
    if (gi_error_sql == 0)
    {
        at_col(rep1,5,"Error: No se pudo consultar periodo estadistico");
        print_line(rep1);
        set_ret_val(-1);
        return(0);
    }

    strcpy(gs_fecharealperiodo, gprvalper->vafereal);

    if (gc_swrec == 'V') 
        sprintf(ls_qryrec, "AND rcviaje = %d", gi_orden);
    else
        sprintf(ls_qryrec, "AND rcorden = %d", gi_orden);
	
    /** JML! Ciclo de recibos por viaje (si gc_swrec = 'R',se procesa 1 vez **/
	
	sprintf(gc_stmsql,
"MULTISELECT(200) rcorden, rcnitcc, rcdepdesp, rcnivel, rccodniv, rcfecha,\
rctotrec, rcultplu, rcvalvta, rcconmer, time_stmp, rcordenc, rcestado, \
nvl((select shipment_type from logfiresvh where file_name=rcobserv2), 'NA') \
FROM trcrecibos \
WHERE rcdepend = %d %s\
AND rctipo = '%s'\
AND rcestado MATCHES '[ZI]'", gi_depend, ls_qryrec, gs_tiporec);

    gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);

    if (gi_error_sql < 0 )
    {
        DB_check_error( gi_error_sql);
        sprintf(dummy_str,"Error en la Base de Datos %d",gi_error_sql);
        at_col(rep1,5,dummy_str);
        print_line(rep1);
        at_col(rep1,5,"Al tratar de leer recibos - encabezado");
        print_line(rep1);
        set_ret_val (gi_error_sql);
        return(-1);
    }
    if (gi_error_sql == 0)
    {
        abort_trans(RECDB);
        at_col(rep1,5,"Error: El Recibo YA fue aprobado");
        print_line(rep1);
        set_ret_val(-1);
        return(0);
    }

    li_numrec = gi_error_sql;

    li_pos = 1;
    for (li_ind = 0; li_ind < li_numrec; li_ind++) 
    {
    	if ( int_get_field(li_pos++, &gprrecibos[li_ind]->rcorden) < 0 ) return(-1);
    	if ( str_get_field(li_pos++, gprrecibos[li_ind]->rcnitcc) < 0 ) return(-1);
    	if ( int_get_field(li_pos++, &gprrecibos[li_ind]->rcdepdesp)< 0 ) return(-1);
    	if ( int_get_field(li_pos++, &gprrecibos[li_ind]->rcnivel) < 0 ) return(-1);
    	if ( int_get_field(li_pos++, &gprrecibos[li_ind]->rccodniv)< 0 ) return(-1);
    	if ( str_get_field(li_pos++, gprrecibos[li_ind]->rcfecha) < 0 ) return(-1);
    	if ( dbl_get_field(li_pos++, &gprrecibos[li_ind]->rctotrec)< 0 ) return(-1);
    	if ( int_get_field(li_pos++, &gprrecibos[li_ind]->rcultplu)< 0 ) return(-1);
    	if ( dbl_get_field(li_pos++, &gprrecibos[li_ind]->rcvalvta)< 0 ) return(-1);
    	if ( int_get_field(li_pos++, &gprrecibos[li_ind]->rcconmer)< 0 ) return(-1);
    	if ( str_get_field(li_pos++, gprrecibos[li_ind]->time_stmp)< 0 ) return(-1);
    	if ( int_get_field(li_pos++, &gprrecibos[li_ind]->rcordenc) < 0 ) return(-1);
    	if (char_get_field(li_pos++, &gprrecibos[li_ind]->rcestado) < 0 ) return(-1);
		if ( str_get_field(li_pos++, gprrecibos[li_ind]->rcobserv2)< 0 ) return(-1);
    }
//nrv
	int li_Tipo_Rec ;
	if (strcmp(gs_tiporec,"IMP") == 0) {
		if (strcmp(gs_anden,"55") == 0)
			strcpy(ls_ubica,"0-02-001-1");
		else{
		
			WriteDebugMsg("Evaluar posicion");
			li_Tipo_Rec = FN_TipoDestCurva(gprrecibos[0]->rcorden);
			if(li_Tipo_Rec == 2) // B (DISTRIBUCION)
				strcpy(ls_ubica,"0-02-001-1");
			else
				strcpy(ls_ubica,"0-01-001-1");
			}
	
	}
	else // "EM
		strcpy(ls_ubica,"1-01-001-1");


/*
	if (strcmp(gs_tiporec,"IMP") == 0) {
		if (strcmp(gs_anden,"55") == 0) 
			strcpy(ls_ubica,"0-02-001-1");
		else
			strcpy(ls_ubica,"0-01-001-1");
	}
	else
		strcpy(ls_ubica,"1-01-001-1");
*/
	WriteDebugMsg("Linea 1887 previo ciclo For");
    for (li_ind = 0; li_ind < li_numrec; li_ind++)
    {
    	gi_orden = gprrecibos[li_ind]->rcorden ;
    	strcpy(gs_nitcc , gprrecibos[li_ind]->rcnitcc );
    	gi_depdes = gprrecibos[li_ind]->rcdepdesp ;
    	gi_nivel = gprrecibos[li_ind]->rcnivel ;
    	gi_codniv = gprrecibos[li_ind]->rccodniv ;
    	strcpy( gs_fecha , gprrecibos[li_ind]->rcfecha );
    	gd_cosrec = gprrecibos[li_ind]->rctotrec ;
    	li_ultplu = gprrecibos[li_ind]->rcultplu ;
    	ld_valvta = gprrecibos[li_ind]->rcvalvta ;
    	li_conmer = gprrecibos[li_ind]->rcconmer ;
    	strcpy(gs_tiempo , gprrecibos[li_ind]->time_stmp );
		// Si tipo de recibo es IMP debe dejar en 0 variable gi_ordcomph para poder buscar costo comex. 
    	gi_ordcomph = gprrecibos[li_ind]->rcordenc;
		gi_ordComex = 0;
		if ( strcmp(gs_tiporec,"IMP") == 0 ) {
		   gi_ordComex = gprrecibos[li_ind]->rcordenc;
		   gi_ordcomph = 0;
		   WriteDebugMsg("IMP: gi_ordcomph igual a 0");
		}
		
    	li_ordcompp = 0;
        lc_cantordp = 'U';
        lc_estado = gprrecibos[li_ind]->rcestado;
		
		//Asigna valores seteados en la carga de la verificacion del ASN.		
		strcpy(ls_tipoflujo, gprrecibos[li_ind]->rcobserv2);

        if (strcmp(gs_tiporec,"RT") == 0)
        {
            sprintf(gc_stmsql, "SELECT denitcc, detipo \
FROM tgndepende A, tdbdespa  B \
WHERE A.dedepend = %d AND B.dedepend = A.dedepend \
AND deorden = %d",
			gi_depdes, gi_ordcomph);

            gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);
            if (gi_error_sql < 0 )
            {
                DB_check_error( gi_error_sql);
                sprintf(dummy_str,"Error en la Base de Datos %d",gi_error_sql);
                at_col(rep1,5,dummy_str);
                print_line(rep1);
                at_col(rep1,5,
                   "Al tratar de leer Nit de dependencia que despacha");
                print_line(rep1);
                set_ret_val (gi_error_sql);
                return(-1);
            }
            if (gi_error_sql == 0)
            {
                abort_trans(RECDB);
                at_col(rep1,5,"Error: No se encontro dependencia que despacha");
                print_line(rep1);
                set_ret_val(-1);
                return(0);
            }
            if ( str_get_field(1, gs_nitcc) < 0 ) return(-1);
            if ( char_get_field(2, &gc_tipotras) < 0 ) return(-1);
        }
        else if (gi_ordcomph > 0)
        {
        	if (strcmp(gs_tiporec,"RF") == 0) 
            /*Buscar O.C. padre, si es Unica o tiene entregas Progr (Muchas) */
            sprintf(gc_stmsql, "SELECT ofconsecu, 'U', offecent FROM tinordfab \
WHERE ofdepend= %d \
AND ofconsecu  = %d ",
                                gi_depend, gi_ordcomph);
			else
            /*Buscar O.C. padre, si es Unica o tiene entregas Progr (Muchas) */
            sprintf(gc_stmsql, "SELECT ocorden, 'U', ocfliminf FROM tcmordenco \
WHERE  ocdepend IN (50,60,55, %d) \
AND (occonsol = ocorden OR 'IMP' = '%s') \
AND ocorden  = %d \
UNION  \
SELECT occonsol,'M', ocfliminf FROM tcmordenco \
WHERE  ocdepend IN (50,60,55,%d) \
AND occonsol != 0 \
AND occonsol != ocorden \
AND 'IMP' <> '%s' \
AND ocorden  = %d ",
                                gi_depend, gs_tiporec,gi_ordcomph, gi_depend, 
                                gs_tiporec,gi_ordcomph);

            gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);
            if (gi_error_sql < 0 )
            {
                DB_check_error( gi_error_sql);
                sprintf(dummy_str,"Error en la Base de Datos %d",gi_error_sql);
                at_col(rep1,5,dummy_str);
                print_line(rep1);
                set_ret_val (gi_error_sql);
                return(-1);
            }
            if (gi_error_sql == 0)
            {
                abort_trans(RECDB);
                at_col(rep1,5,"Error: No se encontro orden de compra");
                print_line(rep1);
                set_ret_val(-1);
                return(0);
            }
            if (strcmp(gs_tiporec,"IMP")==0)
                li_ordcompp = gi_ordcomph;
            else
                if ( DB_int_get_field( 1, &li_ordcompp) < 0 ) return(-1);
            if ( DB_char_get_field( 2, &lc_cantordp) < 0 ) return(-1);
    	    if ( str_get_field( 3, gs_fecord) < 0 ) return(-1);
        }

       


	   /* Validar que Rec.Importado tenga clasificacion de Articulo - Packlist */
        if (strcmp(gs_tiporec,"IMP")==0 && BodegaPosicional(gi_depend) && strcmp(gs_usuario,"LOGFIRE") != 0) {
            sprintf(gc_stmsql, "SELECT epestado FROM trcestpl \
WHERE  epdepend = %d AND eporden = %d",
                                   gi_depend, gi_orden);
            gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);
            if (gi_error_sql < 0 )
            {
                DB_check_error( gi_error_sql);
                sprintf(dummy_str,"Error en la Base de Datos %d",gi_error_sql);
                at_col(rep1,5,dummy_str);
                print_line(rep1);
                set_ret_val(-1);
                return(-1);
            }
            if (gi_error_sql == 0 )
            {
                sprintf(dummy_str,"Recepcion Importado no se ha clasificado",gi_error_sql);
                at_col(rep1,5,dummy_str);
                print_line(rep1);
                set_ret_val(-1);
                return(-1);
			}
		}

        if (gi_depend != 60 && gi_depend != 53 && !BodegaPosicional(gi_depend)) 
            lc_swpos = 'N';
        else {
            if (BodegaPosicional(gi_depend))
            	lc_swpos = 'S';
            else
               lc_swpos = 'N';
        }
		
        if ((gi_PacketID = BeginPacket(CENTRAL_DEPEND,"Aprueba Recibo")) < 0)
        {
            set_ret_val(-1);
            at_col(rep1,5,"Error : no se puede sincronizar");
            print_line(rep1);
            return(-1);
        }

        /* Proceso para aprobar recibo */
        if ( begin_trans(RECDB) < 0 )
        {
            DB_check_error(-2);
            at_col(rep1,5,"Error Base de Datos: al empezar transaccion"); 
            print_line(rep1);
            return(-1);
        }
		//begin_trans(CLIDB);

        /* Obtiene consec del numero de entrada de la aprobacion del recibo */

        if((li_nroent = FN_Gnactsec(186, gi_depend )) <= 0) 
        {
            abort_trans(RECDB);
            PR_InPrtSysErr( rep1, 30107);
            return(-1);
        }

        if (strcmp(gs_tiporec,"RT") == 0)
            gi_codtran = CT_RRCENTMERT;
        else
            gi_codtran = CT_RRCENTMERN;
		
		sprintf(gc_stmsql, "UPDATE trcpacklisn SET pnestado = 'T' \
WHERE pnordenrec = %d \
AND pndepend IN (50,60,55,0,%d) \
AND pntiporec = '%s' \
AND pnestado = 'R' ",
			gi_orden,
			gi_depend, gs_tiporec);
		gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);
	    if (gi_error_sql < 0 )
	    {
	        DB_check_error( gi_error_sql);
	        sprintf(dummy_str,"Error en la Base de Datos %d",gi_error_sql);
	        at_col(rep1,5,dummy_str);
	        print_line(rep1);
	        at_col(rep1,5,"Al tratar de actualizar el Packing List");
	        print_line(rep1);
	        set_ret_val (gi_error_sql);
	        return(-1);
	    }
		
        sprintf (gc_stmsql,"CONTSELECT rdplu, rdcanrec, rdcanbon, rdcandev, \
rdcosto, rdcosrep, rdprelis, rdunidad, rdcambun, \
rcfecha, rchrllegada, rcanden, arcodiva \
FROM trcrecdet, tmpplus, trcrecibos, tmparticul \
WHERE rddepend = %d AND rdorden = %d AND rdtipo = '%s' \
AND rdplu > %d AND plplu = rdplu \
AND rcdepend=rddepend AND rcorden = rdorden \
AND rctipo = rdtipo AND  rdcanrec >= 0 \
AND plarticul = ararticul \
ORDER BY rdplu",
                       gi_depend, gi_orden, gs_tiporec, li_ultplu);

        gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

        if (gi_error_sql < 0 )
        {
            at_col(rep1,5,"Error Base de Datos: Contselect Recibos - Detalle"); 
            print_line(rep1);
            return(-1);
        }

        gd_cosrech =  0.0; /* Variable para estadisticas proveedor */
        gd_vrpresup = 0.0; /* Actualizar presupuesto en caso de requerirlo */
        gc_parcial = 'N';
        gd_canpedida = 0.0;
        ld_totalrec = 0.0;
        li_commit = 0;

        while (gi_error_sql > 0)
        {
            PR_RcSacaPila();
            /* busca precio de venta del plu */
            PR_Inexiplu(gi_depend,gprrecdet->rdplu,&gprinventa);
            if (gi_error_sql <= 0)
            {
                at_col(rep1,5,"Error al buscar registro en inventario. ");
                print_line(rep1);
                abort_trans (RECDB);
                set_ret_val(-1);
                return(-1);
            }
            /* Actualiza detalle de la Orden-Fabricacion detalle */
            if (strcmp(gs_tiporec,"RF") == 0 ) {
                PR_RcActFabDet (gprrecdet->rdplu, gprrecdet->rdcanrec,
                               gprrecdet->rdcanbon, gprrecdet->rdcandev, 
                               gi_ordcomph, lc_cantordp);

                if (gi_error_sql < 0) {
                    at_col(rep1,5,
                    "Error Base de Datos: Actualizar O.Fabric. - Detalle");
                    print_line(rep1);
                    return(-1);
                }
                else if (gi_error_sql == 0) {
                    at_col(rep1,5,"Error al actualizar O.Fabric. - Detalle"); 
                    print_line(rep1);
                    abort_trans (RECDB);
                    set_ret_val(-1);
                    return(0);
                }
            }
 
            /* Actualiza detalle de la Orden-Compra detalle */
            if (strcmp(gs_tiporec,"RT") != 0 &&
                strcmp(gs_tiporec,"RF") != 0 )
            {
                PR_RcActOcDet (gprrecdet->rdplu, gprrecdet->rdcanrec,
                               gprrecdet->rdcanbon, gprrecdet->rdcandev, 
                               li_ordcompp, lc_cantordp);

                if (gi_error_sql < 0) {
                    at_col(rep1,5,
                    "Error Base de Datos: Actualizar O.C. - Detalle");
                    print_line(rep1);
                    return(-1);
                }
                else if (gi_error_sql == 0) {
                    at_col(rep1,5,"Error al actualizar O.C. - Detalle"); 
                    print_line(rep1);
                    abort_trans (RECDB);
                    set_ret_val(-1);
                    return(0);
                }
            }
 
            /* Actualiza detalle del despacho para recepcion tipo RT */
		/*********************** FCM 11/04/2000  *********/
            if (strcmp(gs_tiporec,"RT") == 0) {
                PR_RcActDesdet (gprrecdet->rdplu, gprrecdet->rdcanrec);
                if (gi_error_sql < 0)
                {
                    at_col(rep1,5,
                    "Error Base Datos: al actualizar Despachos- detalle");
                    print_line(rep1);
                    return(-1);
                }
                else
                if (gi_error_sql == 0)
                {
                    at_col(rep1,5,"Error: al actualizar Despachos- detalle");
                    print_line(rep1);
                    abort_trans (RECDB);
                    set_ret_val(-1);
                    return(0);
                }
            }
		    //////////////////////////////////////////////////////////////////
            // Actualizar inventarios:										//
            // - Si Plu es Normal: actualiza normalmente					//
            // - Si PLU es Padre se actualiza el hijo teniendo en cuenta	//
            //  factor padre-hijo											//
            // - Si el recibo es tipo RT, actualiza cantidad en transito	//
            // OJO: Ahora con el PLU padre no se hace nada 15-Jun-2009.		//
		    //////////////////////////////////////////////////////////////////
            if ((li_plhijo = FN_Rcplhijo(gprrecdet->rdplu)) > 0)
				li_plu = li_plhijo;
			else
				li_plu = gprrecdet->rdplu;
			
            // Si recepcion es de operador logistico, obtener costo Comex
            if (strcmp(gs_tiporec,"IMP") == 0 && gi_ordcomph == 0 
            	&& gprrecdet->rdcanrec > 0) {
				
				/* 06-Nov-2015. Si usuario no es LOGFIRE es una aprobacion SICA de lo contrario es WMS. */
				if (strcmp(gs_usuario, "LOGFIRE") != 0)
				{
      				sprintf(gc_stmsql, "SELECT rocodcaja, pl_ccred_id, pl_embcorr, pl_ejercicio \
FROM trcrecopl, OUTER comexpacklis a \
WHERE roplu = %d AND rodepend = %d AND roorden = %d \
AND pl_boxcode = rocodcaja \
AND pl_correlativo = (SELECT MAX(pl_correlativo) FROM comexpacklis WHERE pl_boxcode = rocodcaja)", 
							li_plu, gi_depend, gi_orden);
      				
					sprintf(gc_stmsql, "SELECT rocodcaja, pl_ccred_id, pl_embcorr, pl_ejercicio \
FROM trcrecopl, OUTER comexpacklis a \
WHERE roplu = %d AND rodepend = %d AND roorden = %d \
AND pl_boxcode = rocodcaja \
ORDER BY pl_correlativo DESC", 
							li_plu, gi_depend, gi_orden);
					
       				gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );
					
       				if ( gi_error_sql < 0 ) {
           				DB_check_error( gi_error_sql );
	       				return( gi_error_sql );
       				}
       				else if ( gi_error_sql == 0 ) {
      				   sprintf(gc_stmsql,"Error: Plu %d fue modificado en Packing List", li_plu);
                       at_col(rep1,5,gc_stmsql);
                       print_line(rep1);
                       abort_trans (RECDB);
                       set_ret_val(-1);
                       return(0);
       				}
					
					/* FCM Feb-22-2005  Si la caja no existe se adiciona a 
					   solicitud, por que se puede haber solicitado en el dia */
					/* Recupero el codigo de la caja */
					DB_str_get_field(1, ls_codcaja);
					DB_int_get_field(2, &li_carpeta);
					DB_int_get_field(3, &li_embarque);
					DB_int_get_field(4, &li_ejercicio);

					/* PRC 05-Mar-2009
					Se cambia la forma de registrar las cajas que han sido recibidas
        			sprintf(gc_stmsql,"SELECT plboxcode FROM trcpacklis WHERE plboxcode = '%s' ", ls_codcaja);
					
        			gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);
        			if (gi_error_sql < 0 ) {
                			DB_check_error(gi_error_sql);
                			return;
        			}
        			if (gi_error_sql == 0) {
            			sprintf(gc_stmsql,"INSERT INTO trcpacklis \
						(plejercicio,plboxcode,plccredid,plembcorr,plfbodega,\
						plfllegada,plpriori, time_stmp) \
    					SELECT UNIQUE pl_ejercicio,TRIM(pl_boxcode), \
						pl_ccred_id, pl_embcorr, IncDate('%s',-1),' ',%d,'%s' \
    					FROM   comexpacklis WHERE pl_boxcode = '%s'",
            			"20030101", 0, get_curr_time(), ls_codcaja);
   
           				gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);
    
           				if (gi_error_sql < 0 ) {
                   			DB_check_error(gi_error_sql);
                   			return;
           				}
           				if (gi_error_sql == 0) {
                			abort_trans(RECDB);
                			set_ret_val(NOT_FOUND); 
                			get_msg(30097); // Recepcion duplicada
                			return;
           				}
         			}
         			*/
					
      				sprintf(gc_stmsql, "SELECT orion_costounit, orion_correlativo, orion_costotrib \
FROM trcrecopl, tmpplus, comexcostart \
WHERE roplu = %d AND rodepend = %d AND roorden = %d \
AND plplu = roplu \
AND orion_articulo = plarticul \
AND orion_ccred_id = %d \
AND orion_embcorr = %d \
AND orion_ejercicio = %d \
ORDER BY orion_correlativo DESC ",
						li_plu, gi_depend, gi_orden,
						li_carpeta, li_embarque, li_ejercicio);
					
       				gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );
					
       				if ( gi_error_sql < 0 ) {
           				DB_check_error( gi_error_sql );
	       				return( gi_error_sql );
       				}
       				if ( gi_error_sql == 0 ) {
                       at_col(rep1,5,"Error: al obtener costo COMEX");
                       print_line(rep1);
                       abort_trans (RECDB);
                       set_ret_val(-1);
                       return(0);
       				}
                    if ( dbl_get_field( 1,&(gprrecdet->rdcosrep)) < 0 ) return;
                    if ( dbl_get_field( 3,&(ld_orioncostri)) < 0 ) return;
                    
              
              		sprintf(gc_stmsql, "costo trib  ---------> %.2f  %s  ",ld_orioncostri,gs_tiporec);
					        WriteDebugMsg(gc_stmsql);
                    
                    gprrecdet->rdcosto = gprrecdet->rdcosrep * FN_RcIvaArt('P', li_plu);
                    
                 /** 17-Jun-2009. MVM: No debe incrementar IVA para este tipo de costo **
                    ld_orioncostri = ld_orioncostri * GC_IVA;	// No se debe utilizar esta constante PRC 26-Jul-2010
                  **/
                                		
              		sprintf(gc_stmsql, "costo trib SIN + IVA  ---------> %.2f ",ld_orioncostri,gs_tiporec);
					        WriteDebugMsg(gc_stmsql);
                    
                    if ( gprrecdet->rdcosto <= 0.0 || gprrecdet->rdcosto > 2000000 ) {
                       sprintf(gc_stmsql,
                          "Error: Plu %d tiene costo COMEX en cero o es muy alto : Costo :%.0f", li_plu , gprrecdet->rdcosto);
                       at_col(rep1,5, gc_stmsql);
                       print_line(rep1);
                       abort_trans (RECDB);
                       set_ret_val(-1);
                       return(0);
                    }
				}
				// Es Aprobacion WMS
				else 
				{
      				sprintf(gc_stmsql, "SELECT orion_costounit, orion_correlativo, orion_costotrib \
FROM tmpplus, comexcostart \
WHERE plplu = %d \
AND orion_depend IN (50,%d) \
AND orion_orden = %d \
AND orion_articulo = plarticul \
AND orion_ccred_id = %d \
AND orion_ejercicio = %d \
AND orion_embcorr = %d \
ORDER BY orion_correlativo DESC ", li_plu, gi_depend, gi_ordComex, gi_credId, gi_ejerc, gi_embcorr);
					
       				gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );
					
       				if ( gi_error_sql < 0 ) {
           				DB_check_error( gi_error_sql );
	       				return( gi_error_sql );
       				}
       				if ( gi_error_sql == 0 ) {
                       at_col(rep1,5,"Error: al obtener costo COMEX");
                       print_line(rep1);
                       abort_trans (RECDB);
                       set_ret_val(-1);
                       return(0);
       				}
                    if ( dbl_get_field( 1,&(gprrecdet->rdcosrep)) < 0 ) return;
                    if ( dbl_get_field( 3,&(ld_orioncostri))      < 0 ) return;
                    
              
              		sprintf(gc_stmsql, "costo trib  ---------> %.2f  %s  ",ld_orioncostri,gs_tiporec);
					        WriteDebugMsg(gc_stmsql);
                    
                    gprrecdet->rdcosto = gprrecdet->rdcosrep * FN_RcIvaArt('P', li_plu);
                                		
              		sprintf(gc_stmsql, "costo trib SIN + IVA  ---------> %.2f ",ld_orioncostri,gs_tiporec);
					        WriteDebugMsg(gc_stmsql);
                    
                    if ( gprrecdet->rdcosto <= 0.0 || gprrecdet->rdcosto > 2000000 ) {
                       sprintf(gc_stmsql,
                          "Error: Plu %d tiene costo COMEX en cero o es muy alto : Costo :%.0f", li_plu , gprrecdet->rdcosto);
                       at_col(rep1,5, gc_stmsql);
                       print_line(rep1);
                       abort_trans (RECDB);
                       set_ret_val(-1);
                       return(0);
                    }
				}
			 }

            /* Toma caso para Recepciones sin OC y RT valor cero */

            if (gc_parcial == 'N' && gi_ordcomph > 0)
                ld_ordena = gd_canpedida * gi_factorph;
            else
                ld_ordena = (gprrecdet->rdcanrec + gprrecdet->rdcanbon) *
                             gi_factorph;

            ld_total = gprrecdet->rdcanrec + gprrecdet->rdcanbon;
            ld_totalrec += ld_total;

            /* Actualiza inventarios:
            - Suma existencias
            - Resta ordenadas  (Solo para recepciones con OC)
            - Resta en transito (solo para recepciones RT) 
            - Costos: ultimo, minimo, maximo y promedio */
            /******************* Abril 17/95 Redisenno MZP
            Se incluyo paso de unidad de empaque y costo unidad de empaque
            para actualizar los costos de inventario **********************/


           	if (strcmp(gs_tiporec,"RT") == 0) {
				if (gc_tipotras == 'N') {
           	    	li_sw = FN_InRecInv (gi_depend, li_plu, ld_total * gi_factorph,
           	    	0.0, -gd_cantdesp , gprrecdet->rdcosrep/gi_factorph, 
           	                    gprrecdet->rdcosto/gi_factorph,
					gprrecdet->rdunidad, gs_fecha, gs_tiporec, 
					gi_PacketID);
				}
				else if (gc_tipotras == 'S') {
      				sprintf(gc_stmsql, 
"UPDATE tinventa SET inexisten = inexisten + %.3f, \
intransit = intransit - %.3f \
WHERE independ = %d \
AND inplu = %d", gd_cantdesp,gd_cantdesp,gd_cantdesp, 
						gs_fecha, gi_depend, li_plu);

       				gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );

       				if ( gi_error_sql < 0 ) {
           				DB_check_error( gi_error_sql );
	       				return( gi_error_sql );
       				}
       				if ( gi_error_sql == 0 ) {
           				set_ret_val(0);
           				get_msg(41405); 
           				return(0);
       				}
      				sprintf(gc_stmsql, 
"UPDATE tinvenest SET iefecultr='%s' \
WHERE iedepend = %d \
AND ieplu = %d", gs_fecha, gi_depend, li_plu);

       				gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );

       				if ( gi_error_sql < 0 ) {
           				DB_check_error( gi_error_sql );
	       				return( gi_error_sql );
       				}
       				if ( gi_error_sql == 0 ) {
           				set_ret_val(0);
           				get_msg(41405); 
           				return(0);
       				}
                   	/* Se cambia el estado del Servicio Tecnico */
      				sprintf(gc_stmsql, 
"UPDATE tinsertec SET stestado = 'P' \
WHERE stdepend = %d \
AND stplu = %d AND stconsecu = %d", 
			 		gi_depend, li_plu, gi_nserie);

       				gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );

       				if ( gi_error_sql < 0 ) {
           				DB_check_error( gi_error_sql );
	       				return( gi_error_sql );
       				}
       				if ( gi_error_sql == 0 ) {
           				set_ret_val(0);
           				get_msg(41405); 
           				return(0);
       				}
				}
				else if (gc_tipotras == 'R') {
      				sprintf(gc_stmsql, 
"UPDATE tinventa SET inexisten = inexisten + %.3f, \
intransit = intransit - %.3f \
WHERE independ = %d \
AND inplu = %d", gd_cantdesp, gd_cantdesp, 
				 	gi_depend, li_plu);

       				gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );

       				if ( gi_error_sql < 0 ) {
            			DB_check_error( gi_error_sql );
	        			return( gi_error_sql );
       				}
       				if ( gi_error_sql == 0 ) {
            			set_ret_val(0);
            			get_msg(41405);  
            			return(0);
       				}
      				sprintf(gc_stmsql, 
"UPDATE tinvenest SET iefecultr='%s' \
WHERE iedepend = %d \
AND ieplu = %d", gs_fecha, gi_depend, li_plu);

       				gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );

       				if ( gi_error_sql < 0 ) {
            			DB_check_error( gi_error_sql );
	        			return( gi_error_sql );
       				}
       				if ( gi_error_sql == 0 ) {
            			set_ret_val(0);
            			get_msg(41405);  
            			return(0);
       				}
                   	/* Se cambia el estado del Servicio Tecnico */
      				sprintf(gc_stmsql, 
"UPDATE tinsertec SET stestado = 'F' \
WHERE stdepsol = %d \
AND stplu = %d AND stconsecu = %d", 
				 	gi_depend, li_plu, gi_nserie);

       				gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );

       				if ( gi_error_sql < 0 ) {
            			DB_check_error( gi_error_sql );
	        			return( gi_error_sql );
       				}
       				else if ( gi_error_sql == 0 ) {
            			set_ret_val(0);
            			get_msg(41405);
            			return(0);
       				}
				}
           	}
           	else {
           		if (strcmp(gs_tiporec,"RF") != 0) 
                   	ld_costplures = 0.00;
               	else if (ld_total > 0) {
                     	/** Obtener costo de inventario para plu resultante **/
                     	sprintf(gc_stmsql, "SELECT SUM(vlcostuni * ficantins) \
FROM tinventa, tinordfadd, tinvaloriza \
WHERE fidepend = %d \
AND ficonsecu = %d \
AND fiplures =  %d \
AND inplu = fipluins AND vlplu = fipluins \
AND independ = fidepend",
                    	gi_depend, gi_ordcomph, li_plu );

                    	gi_error_sql = DB_exec_sql(RECDB, gc_stmsql);

                    	if (gi_error_sql < 0) {
                    		at_col(rep1,5,
                    		"Error B D: al actualizar Despachos- detalle");
                    		print_line(rep1);
                    		return(-1);
                    	}
                    	else if (gi_error_sql == 0) {
                    		at_col(rep1,5,
							"Error: al actualizar Despachos- detalle");
                    		print_line(rep1);
                    		abort_trans (RECDB);
                    		set_ret_val(-1);
                    		return(0);
						}
						if (DB_dbl_get_field(1, &ld_costinsum) < 0) return;

						ld_costplures = ld_costinsum ;
                	
      					sprintf(gc_stmsql,"MULTISELECT(10) fipluins, ficantins \
FROM tinordfadd \
WHERE fiplures = %d \
AND fidepend = %d \
AND ficonsecu= %d", 
				 			li_plu, gi_depend, gi_ordcomph); 

       					gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );
       					if ( gi_error_sql < 0 ) {
            				DB_check_error( gi_error_sql );
	        				return( gi_error_sql );
       					}
       					else if ( gi_error_sql == 0 ) {
							set_ret_val(0);
            				get_msg(41405); 
            				return(0);
       					}
						li_totinsumos = gi_error_sql;
						for (li_ins=0; li_ins < li_totinsumos; li_ins++) {
							DB_int_get_field(li_ins*2+1,&ls_pluins[li_ins].plu);
							DB_dbl_get_field(li_ins*2+2,&ls_pluins[li_ins].cant);
						}	

						for (li_ins=0; li_ins < li_totinsumos; li_ins++) {
							/* Incremento el plu insumo por transacion interna */
                			PR_InRegLogTran(gi_depend, ls_pluins[li_ins].plu,
                        	ld_total*ls_pluins[li_ins].cant,CT_ANENVIOINSUMOS,
							gi_orden);
            				if (gi_error_sql < 0) {
                				at_col(rep1,5,
                				"Error B Datos: al actualizar Log Trans."); 
                				print_line(rep1);
                				set_ret_val (li_sw);
                				return(-1);
            				}
            				else if ( gi_error_sql == 0) {
                				at_col(rep1,5,"Error: al actualizar Log Trans."); 
                				print_line(rep1);
                				abort_trans(RECDB);
                				set_ret_val (-1);
                				return(0);
            				}

							/* Decremento el plu insumo por trans externa */
                			PR_InRegLogTran(gi_depend, ls_pluins[li_ins].plu,
                        	(-1)*ld_total*ls_pluins[li_ins].cant, 
							CT_RECEPINSUMOS, gi_orden);
            				if (gi_error_sql < 0) {
                				at_col(rep1,5,
                				"Error Base de Datos: al actualizar Log Trans."); 
                				print_line(rep1);
                				set_ret_val (li_sw);
                				return(-1);
            				}
            				else if ( gi_error_sql == 0) {
                				at_col(rep1,5,"Error: al actualizar Log Trans."); 
                				print_line(rep1);
                				abort_trans(RECDB);
                				set_ret_val (-1);
                				return(0);
            				}

						}
      					sprintf(gc_stmsql, 
"UPDATE tinventa SET inenproce = inenproce - \
(SELECT ficantins * %.2f FROM tinordfadd \
WHERE fiplures = %d \
AND   fipluins = inplu \
AND   fidepend = %d \
AND   ficonsecu= %d) \
WHERE independ = %d \
AND   inplu IN \
(SELECT fipluins FROM tinordfadd \
WHERE fiplures = %d \
AND   fidepend = %d \
AND   ficonsecu= %d)",
				 			ld_total, li_plu, gi_depend, gi_ordcomph, 
							gi_depend,
				 			li_plu, gi_depend, gi_ordcomph);

       					gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );

       					if ( gi_error_sql < 0 ) {
            				DB_check_error( gi_error_sql );
	        				return( gi_error_sql );
       					}
       					if ( gi_error_sql == 0 ) {
            				set_ret_val(0);
            				get_msg(41405);   /* No pudo actualizar inventarios */
            				return(0);
       					}

					}	

                    if (lc_swpos == 'S') {
		                if (PR_InPosicionar(li_plu, ld_total*gi_factorph, gi_depend, 
			               ls_ubica, 0, 'N') <= 0) {
                           abort_trans(INVDB);
                           set_ret_val(-1);
                           return;
                       }

                    }

					// Si el recibo es por LogFire, bloqueamos cantidad recibida.
					if ( strcmp(gs_usuario, "LOGFIRE") == 0 && strcmp(ls_tipoflujo, "VEV") != 0 )
					{
						sprintf(gc_stmsql, "UPDATE tinventa SET \
inbloqueado = inbloqueado + %.2f \
WHERE inplu = %d \
AND independ = %d", ld_total * gi_factorph, li_plu, gi_depend);

       					gi_error_sql = DB_exec_sql( INVDB, gc_stmsql );

       					if ( gi_error_sql < 0 ) {
            				DB_check_error( gi_error_sql );
							abort_trans(INVDB);
	        				return( gi_error_sql );
       					}
					}

					sprintf(gc_stmsql, "PDL-> li_plu:[%d], ld_total:[%.2f], gi_factorph:[%d], ld_ordena:[%.2f]",
						li_plu, ld_total, gi_factorph, ld_ordena);
					WriteDebugMsg(gc_stmsql);

       				li_sw = FN_InRecInv (gi_depend, li_plu, 
						ld_total * gi_factorph, - ld_ordena,  ld_orioncostri/gi_factorph + ld_costplures, 
						gprrecdet->rdcosrep/gi_factorph + ld_costplures, 
                        gprrecdet->rdcosto/gi_factorph + ld_costplures,
						gprrecdet->rdunidad, gs_fecha, gs_tiporec, 
						gi_PacketID);
						
					
      
           	}
           	if (li_sw < 0) {
               	at_col(rep1,5,"Error Base de Datos: al actualizar Inventario"); 
               	print_line(rep1);
               	set_ret_val (li_sw);
               	return(-1);
           	}
           	else if ( li_sw == 0) {
               	at_col(rep1,5,"Error: al actualizar Inventario"); 
               	print_line(rep1);
               	abort_trans(RECDB);
               	set_ret_val (-1);
               	return(0);
           	}

			if (ld_total > 0 ) {
                 
				if (CumplimientoVentaEnVerde(gi_depend, li_plu, ld_total, ls_tipoflujo, gi_orden, gs_tiporec) < 0)
				{
					at_col(rep1, 5, "Error al actualizar Cumplimiento Venta en Verde."); 
					print_line(rep1);
					set_ret_val(-1);
					return(-1);
				}
				
				if (strcmp(gs_tiporec,"RT") == 0)
					gi_codtran = CT_RRCENTMERT;
				else {
					if (gi_codiva != 3)
						gi_codtran = CT_RRCENTMERN;
					else
						gi_codtran = 94;
				}
                /** Se llama a FN_InRegLogTranRev para registrar 
                costo de recibo para recepciones RT JML! **/
        	    if (strcmp(gs_tiporec,"RT") == 0)
                   PR_InRegLogTranRev(gi_depend, li_plu,
                         ld_total * gi_factorph, gi_codtran,gi_orden,
                         gprrecdet->rdcosto/FN_RcIvaArt('P', li_plu));
                else
                   PR_InRegLogTran(gi_depend, li_plu,
                         ld_total * gi_factorph, gi_codtran,gi_orden);

            	if (gi_error_sql < 0) {
                	at_col(rep1,5,
                	"Error Base de Datos: al actualizar Log Transacciones"); 
                	print_line(rep1);
                	set_ret_val (li_sw);
                	return(-1);
            	}
            	else if ( gi_error_sql == 0) {
                	at_col(rep1,5,"Error: al actualizar Log Transacciones"); 
                	print_line(rep1);
                	abort_trans(RECDB);
                	set_ret_val (-1);
                	return(0);
            	}
            	ld_valvta+= (gprrecdet->rdcanrec * gprinventa->inprecven);

            	/* Actualiza el precio de venta en recibo-detalle */
            	sprintf (gc_stmsql,"UPDATE trcrecdet SET rdprevta = %.3f, \
rdcosto = %.3f, rdcosrep = %.3f, \
rdcostri = %.3f \
WHERE rddepend = %d AND rdorden = %d \
AND rdtipo = '%s' AND rdplu = %d",
            	gprinventa->inprecven, 
                gprrecdet->rdcosto,
                gprrecdet->rdcosrep,
				ld_orioncostri, 
                gi_depend, gi_orden, gs_tiporec, 
            	gprrecdet->rdplu);

            	gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

            	if (gi_error_sql < 0 ) {
             		at_col(rep1,5,"Error Base Datos: al act Recibos - Detalle");
                	print_line(rep1);
                	DB_check_error( gi_error_sql);
                	return(-1);
            	}
            	else if (gi_error_sql == 0) {
                	at_col(rep1,5,"Error: al actualizar Recibos - Detalle");
                	print_line(rep1);
                	abort_trans(RECDB);
                	set_ret_val(-1);
                	return(0);
            	}

            	/* Actualiza tabla de Precios segun recibo  */
            	li_funpre =FN_GpVigPreciosR (gi_depend, gprrecdet->rdplu,
                                        'R', gi_PacketID);
            	if (li_funpre < 0) {
               	 at_col(rep1,5,
               	 "Error Base Datos: al actualizar Precios Vigentes");
               	 print_line(rep1);
               	 return(-1);
            	} 
				/* FCM 17/04/02 Solo se encuentran los registros 
				   con cantidad recibida  > 0 ****/
            	li_commit = li_commit + 1;
			}

            /* Obtiene el costo de la mercancia rechazada */
            gd_cosrech += (gprrecdet->rdcandev * gprrecdet->rdcosto);
    
            if (li_commit == CCOMMIT) {
                li_ultplu = gprrecdet->rdplu;

                PR_RcActEstRec (gi_depend, gi_orden, gs_tiporec, ld_valvta,
                                li_ultplu, li_conmer);
				if (gi_error_sql <= 0) {
                	abort_trans(RECDB);
                    at_col(rep1,5,
                    "Error de sincronizacion: Intente nuevamente"); 
                    print_line(rep1);
                    return(-1);
                }
                li_commit = 0;
                end_trans(RECDB);
				//end_trans(CLIDB);
                EndPacket(gi_PacketID);
                if ((gi_PacketID = 
                     BeginPacket(CENTRAL_DEPEND,"Aprueba Recibo"))<0)
                {
                    at_col(rep1,5,"Error : no se puede sincronizar"); 
                    print_line(rep1);
                    return(-1);
                }
                if ( begin_trans(RECDB) < 0 )
                {
                    DB_check_error(-2);
                    at_col(rep1,5,
                    "Error Base de Datos: al empezar transaccion"); 
                    print_line(rep1);
                    return(-1);
                }
			//	begin_trans(CLIDB);
            }
            gi_error_sql = DB_exec_sql (RECDB, NULL);
        }

        /* Actualiza encabezado de Despachos */
        if (strcmp(gs_tiporec,"RT") == 0)
        {
            PR_RcActDes (gi_depend, gi_depdes, gi_orden, gs_tiporec);
            if (gi_error_sql < 0)
            {
                at_col(rep1,5,"Error Base de Datos: al actualizar Despachos");
                print_line(rep1);
                return(-1);
            }
            else if (gi_error_sql == 0)
            {
                at_col(rep1,5,"Error : al actualizar Despachos");
                print_line(rep1);
                abort_trans(RECDB);
                set_ret_val (-1);
                return(-1);
            }
        }

    	/** Actualiza totrec y valvta   **/
    	PR_RcActTotVal(gi_depend, gi_orden, gs_tiporec, &gd_cosrec, &ld_valvta);
    	if (gi_error_sql <= 0 ) {
        	abort_trans(RECDB);
        	return;
    	}

        gi_codniv1 = FN_RcNivel1(gi_nivel, gi_codniv);
        if (strcmp(gs_tiporec,"RT") != 0 && gi_ordcomph > 0)
        {
        	if (strcmp(gs_tiporec,"RF") != 0 ) {
            	/* Actualiza encabezado Ordenes de Compra */
            	PR_RcActOc (gi_depend, gi_orden, gs_tiporec);
            	if (gi_error_sql < 0)
            	{
                	at_col(rep1,5,"Error Base Datos : al actualizar OCompra");
                	print_line(rep1);
                	return(-1);
            	}
            	else
            	if (gi_error_sql == 0)
            	{
                	at_col(rep1,5,"Error : al actualizar OCompra");
                	print_line(rep1);
                	abort_trans (RECDB);
                	set_ret_val (-1);
                	return(-1);
            	}
			}

            /* Actualiza estadisticas del Proveedor */
            gi_codniv1 = FN_RcNivel1(gi_nivel, gi_codniv);

            /*  Obtener registro inicializado de ESTADISTICAS PROVEEDOR *****/
            PR_CmGetEstPr ( &gprestprov );

            /*  Mover datos CLAVE para actualizar las estadisticas ********/
            strcpy ( gprestprov->epnitcc, gs_nitcc);
            gprestprov->epdepend = gi_depend;
            gprestprov->epcodniv = gi_codniv1;
            gprestprov->epperiodo = gprvalper->vaperiodo;

            /*  Mueve los valores actualizar */
            gprestprov->epcontrec = 1; /*Incrementa contador de recibos */
            gprestprov->epcosrech = gd_cosrech; /*Costo cantidad rechazada */
            gprestprov->epcosrec = gd_cosrec; /* Costo cantidad recibida */

            /*  Actualizar ESTADISTICAS PROVEEDOR y validar retorno ********/
            li_retfun = FN_CmEstProvR( &gprestprov, gi_PacketID ); 

            if (li_retfun < 0)
            {
                at_col(rep1,5,
                "Error Base de Datos : al actualizar Estadisticas-Proveedor");
                print_line(rep1);
                return(-1);
            }
            else
            if (li_retfun == 0)
            {
                at_col(rep1,5,"Error : al actualizar Estadisticas-Proveedor");
                print_line(rep1);
                abort_trans (RECDB);
                set_ret_val (-1);
                return(0);
            }
			WriteDebugMsg("SE CAYO1");
			
			/**Presupuesto para O.Fabric. **/
        	if (strcmp(gs_tiporec,"RF") == 0 ) {
            	li_retfun = FN_CmActPptoL(gi_nivel, gi_codniv1, 
								'R', gs_fecord, gs_fecha,  gs_nitcc);
            	if (li_retfun < 0)
            	{
                	at_col(rep1,5,"Error Base de Datos: al actualizar Presupuesto");
                	print_line(rep1);
                	return(-1);
            	}
            	if (li_retfun == 0)
            	{
                	at_col(rep1,5,"Error : al actualizar Presupuesto");
                	print_line(rep1);
                	abort_trans (RECDB);
                	set_ret_val (-1);
                	return(0);
            	}
			}
			else {

            	sprintf (gc_stmsql,"SELECT ocnivapr, ocplazo, ocfecpag, \
ocfliminf, ppplazo1 \
FROM tcmordenco, OUTER tgnplazop \
WHERE ocdepend IN (50,60,55,%d) AND ocorden = %d \
AND ppcodigo = ocplazo", 
            	gi_depend, gi_ordcomph); 

            	gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);
	
            	if (gi_error_sql < 0 ) {
             		at_col(rep1,5,"Error Base Datos: al actualizar Recibos - Detalle");
                	print_line(rep1);
                	DB_check_error( gi_error_sql);
                	return(-1);
            	}
            	else if (gi_error_sql == 0) {
                	at_col(rep1,5,"Error: al actualizar Recibos - Detalle");
                	print_line(rep1);
                	abort_trans(RECDB);
                	return(-1);
            	}
				DB_int_get_field(1, &li_nivapr);
				DB_int_get_field(2, &li_ocplazo);
				DB_str_get_field(3, ls_fecpag);
				DB_str_get_field(4, ls_fliminf);
				if (li_ocplazo > 0 )
					DB_int_get_field(5, &li_plazo);

					WriteDebugMsg("SE CAYO2");
            	/* Actualizar presupuesto */
            	li_retfun = FN_CmActPptoL((int)(li_nivapr/10000), 
							li_nivapr%10000, 'R', gs_fecord, 
                            gs_fecha,  gs_nitcc);
            	if (li_retfun < 0)
            	{
                	at_col(rep1,5,"Error Base de Datos: al actualizar Presupuesto");
                	print_line(rep1);
                	return(-1);
            	}
            	if (li_retfun == 0)
            	{
                	at_col(rep1,5,"Error : al actualizar Presupuesto");
                	print_line(rep1);
                	abort_trans (RECDB);
                	set_ret_val (-1);
                	return(0);
            	}
            	if (li_retfun == 99)
            	{
                	li_retfun = FN_CmObtNodo((int)(li_nivapr/10000), 
                                          li_nivapr%10000,'N');
                	li_retfun = 99;
            	}

    	    	if (gd_vrpresup > 0.0)
            	{
                	if (li_retfun == 99) {
                    	li_retfun = FN_CmObtNodo((int)(li_nivapr/10000), 
                                              li_nivapr%10000,'S');
	                }
                	else{
       	            	/* Actualizar presupuesto  de los comprado que ya no se va 
                       	ha recibir. */
            	    	li_retfun = FN_CmActPptoL((int)(li_nivapr/10000),
                                             li_nivapr%10000,'E', gs_fecord, 
                                         gs_fecha,  gs_nitcc);
                    	if (li_retfun < 0) {
                        	at_col(rep1,5,"Error : al actualizar Presupuesto");
                        	print_line(rep1);
                        	return(-1);
                    	}
                    	else if (li_retfun == 0) {
            	        	at_col(rep1,5,"Error : al actualizar Presupuesto");
            	        	print_line(rep1);
            	        	abort_trans (RECDB);
            	        	set_ret_val (-1);
            	        	return(0);
                    	}

     					if (li_ocplazo > 0 ) {
							if (strcmp(gs_fecha, ls_fliminf) > 0)
								strcpy(ls_fliminf, gs_fecha); 
      						li_fecha = date_to_num(ls_fliminf);
       						li_fecha += li_plazo;
       						num_to_date(li_fecha, ls_fecpag);
	 					}

                        /* Actualiza el presupuesto de pagos */
     					if ( FN_CmActPptoP(li_nivapr/10000, li_nivapr%10000,
										'E', ls_fecpag, -gd_vrpresup,
					  					gs_nitcc ) <= 0 ) {
            	        	at_col(rep1,5,"Error:Actualizar Presupuesto de pago");
            	        	print_line(rep1);
            	        	abort_trans (RECDB);
            	        	set_ret_val (-1);
            	        	return(0);
     					}
                	}
				}
				/** Actualizar Planilla de Citas  JML!2000/06/02 **/
				if ((gi_depend == 50 || gi_depend == 55 || gi_depend == 60) && 
                     strcmp(gs_tiporec,"EM") == 0 ) {
                    if (ld_totalrec == 0 && lc_estado != 'Z' && li_nivapr != 10048 && gi_ordcomph != 99238) {
             		   at_col(rep1,5,"Error : No se han ingresado cantidades recibidas");
                	   print_line(rep1);
                    	return(-1);
                    }
                    li_packing = 0;
	            	sprintf (gc_stmsql,"SELECT pndepend \
FROM trcpacklisn \
WHERE pnordenrec = %d \
AND pndepend IN (50,60,55,0,%d) \
AND pntiporec = '%s' ",
						gi_orden,
						gi_depend, gs_tiporec);
	            	gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);
					
	            	if (gi_error_sql < 0 ) {
	             		at_col(rep1,5,"Error Base Datos: al revisar si tiene Packing List");
	                	print_line(rep1);
	                	DB_check_error( gi_error_sql);
	                	return(-1);
	            	}
	            	else if (gi_error_sql > 0) {
	            		li_packing = 1;
	            	}
					
					if (li_packing != 1 && gprparamet->papais == CHILE && strcmp(gs_usuario, "LOGFIRE") != 0) 
					{
						/* Buscar Datos de Cita */
	            	    sprintf (gc_stmsql, "SELECT pounipro, pcmodulo, pcmodurec, pccantrec \
FROM trcplancoc, trcplancit \
WHERE podepend = %d AND poorden = %d AND pofecha = '%s' \
AND   pcdepend = podepend \
AND   pcfecha = pofecha \
AND   pcunipro = pounipro \
AND   pcmodulo = pomodulo",
	            	   		gi_depend, gi_ordcomph, gs_fecha); 
	
	            	   gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);
		
	            	   if (gi_error_sql < 0 ) {
	                	   DB_check_error( gi_error_sql);
	             		   at_col(rep1,5,"Error Base Datos: al actualizar Planilla Citas");
	                	   print_line(rep1);
	                	   return(-1);
	            	   }
	            	   else if (gi_error_sql == 0) {
	                	   abort_trans(RECDB);
	                	   at_col(rep1,5,"Error: al actualizar Planilla Citas");
	                	   print_line(rep1);
	                	   return(-1);
					   }
						int_get_field(1, &reg_citas.unipro);
						int_get_field(2, &reg_citas.modulo);
						int_get_field(3, &reg_citas.modurec);
						dbl_get_field(4, &reg_citas.cantrec);
	
						if (reg_citas.modurec > 0)  
							strcpy(ls_upmodrec," ");
						else {
							/* Actualizar modulo recibo */
							sprintf(ls_upmodrec,", pcmodurec = modulorec('%s')",
								gs_horarec);
						}
						/* Actualizar Datos de Cita */
	            	   sprintf (gc_stmsql,
"UPDATE trcplancit SET pccantrec = pccantrec + %.2f %s \
WHERE pcdepend = %d AND pcunipro = %d AND pcmodulo = %d \
AND pcfecha = '%s'", ld_totalrec, ls_upmodrec,
	            	   		gi_depend, 
						    reg_citas.unipro,
						    reg_citas.modulo,
							gs_fecha); 
	
	            	   gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);
		
	            	   if (gi_error_sql < 0 ) {
	             		   at_col(rep1,5,"Error Base Datos: al actualizar Planilla Citas");
	                	   print_line(rep1);
	                	   DB_check_error( gi_error_sql);
	                	   return(-1);
	            	   }
	            	   else if (gi_error_sql == 0) {
	                	   at_col(rep1,5,"Error: al actualizar Planilla Citas");
	                	   print_line(rep1);
	                	   abort_trans(RECDB);
	                	   return(-1);
					   }
					}
				}
            }
        }
        /* Actualizar Packing List para Recepcion de Operador Logistico**/
        if (strcmp(gs_tiporec,"IMP") == 0 && gi_ordcomph == 0) {
			// Se realiza el pareo en el inicio de recepción 30-Jul-2009
        	if (FN_ProcesarCajas(gi_depend, gi_orden, 'S') != 1)
        	{
	            at_col(rep1, 5, "Error : al actualizar las fechas de recepción de las cajas del Recibo");
	            print_line(rep1);
	            abort_trans (RECDB);
	            set_ret_val (-1);
	            return(0);
        	}
        	/*
			// Actualizar fecha de recepcion bodega
			sprintf (gc_stmsql, "UPDATE trcpacklis SET plfllegada = '%s' \
WHERE plboxcode IN (select rocodcaja from trcrecopl where rodepend = %d and roorden = %d)", 
					OrionDateGet(0), 
					gi_depend, gi_orden);
			
			gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);
			
			if (gi_error_sql < 0 ) {
				at_col(rep1,5,"Error Base Datos: al actualizar Packing List");
				print_line(rep1);
				DB_check_error( gi_error_sql);
				return(-1);
			}
			*/
        }

        sprintf (gc_stmsql, "SELECT rcfecha, rctotrec, rcmodrecep FROM trcrecibos \
WHERE rcdepend = %d \
AND rcorden = %d \
AND rctipo = '%s'",
                             gi_depend, gi_orden, gs_tiporec);

        gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

        if (gi_error_sql < 0 ) {
            DB_check_error( gi_error_sql);
            return(-1);
        }
        else if (gi_error_sql == 0) {
            abort_trans(RECDB);
            get_msg(30324);
            set_ret_val(NOT_FOUND);
            return(0);
        }
        DB_str_get_field(1,gs_fecha);
        DB_dbl_get_field(2,&gd_totrec);
        DB_char_get_field(3,&gc_modrecep);

        gd_compen = gd_totrec;
	
        /********************************************************************/
        /* Actualiza valor de venta del recibo */
        if ( strcmp(gs_tiporec,"IMP") == 0 && gi_ordcomph == 0 ) {
		   strcpy(ls_tipoRecep,"OPL");
           PR_RcActRec (gi_depend, gi_orden, ls_tipoRecep, li_nroent);
		}
        else {
           PR_RcActRec (gi_depend, gi_orden, gs_tiporec, li_nroent);
		}
		
        if (gi_error_sql < 0)
        {
            at_col(rep1,5,
            "Error Base Datos : al actualizar Total venta en Recibo");
            print_line(rep1);
            return(-1);
        }
        else if (gi_error_sql == 0) {
            at_col(rep1,5,"Error : al actualizar Total venta en Recibo");
            print_line(rep1);
            abort_trans (RECDB);
            set_ret_val (-1);
            return(0);
        }

	    /* Actualiza la temporada del articulo */
	    sprintf(gc_stmsql,"UPDATE tmparticul SET artempora = artempora[1,1]||TemporadaVig(artempora[1,1])||'N', time_stmp = '%s' \
WHERE ararticul in (SELECT UNIQUE paarticul \
FROM tmpnaartplu, trcrecdet \
WHERE rdplu = paplu AND rddepend = %d \
AND rdorden = %d AND rdtipo = '%s')", 
	    get_curr_time(),gi_depend, gi_orden, gs_tiporec);

        gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

        if (gi_error_sql < 0 ) {
    	    DB_check_error( gi_error_sql);
            return(-1);
        }

		lc_octipo = 'N';
		if (gi_ordcomph > 0 && gc_swrec != 'V' && gc_swrec != 'X' && gc_swrec != 'S' && gc_swrec != 'Z')  
		{
			sprintf(gc_stmsql, "SELECT octipo FROM tcmordenco WHERE ocorden = %d", gi_ordcomph);

			gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

			if (gi_error_sql < 0 ) {
				DB_check_error( gi_error_sql);
				return(-1);
			}
			else if (gi_error_sql > 0 ) {
				if (DB_char_get_field(1, &lc_octipo) < 0) return -1;
			}
		}

		if (lc_octipo != 'V')
		{

			/* Actuliza las ordenadas de todos los plus del recibos */
			sprintf(gc_stmsql,"UPDATE tinventa SET inordena = ordenadas(inplu, independ) \
WHERE independ IN (60,50,55,69,%d) \
AND  inplu IN (SELECT rdplu FROM trcrecdet WHERE  rddepend = %d \
AND rdorden = %d AND rdtipo = '%s')", 
	    		gi_depend, gi_depend, gi_orden, gs_tiporec);

			gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);

			if (gi_error_sql < 0 ) {
				DB_check_error( gi_error_sql);
				return(-1);
			}
		}

		/* Actuliza las ordenadas de todos los plus del recibos */

        if (strcmp(gs_tiporec,"RF") != 0 &&
            strcmp(gs_tiporec,"RT") != 0 ) {
                gi_ordend=0;

			if (strcmp(gs_usuario, "LOGFIRE") != 0) 
			{
				if (strcmp(gs_tiporec,"IMP") != 0 || gi_depend != 55) 
				{
					if (strcmp(gs_tiporec,"IMP") == 0 && gi_ordcomph == 0) 
					{
						if (BodegaPosicional(gi_depend) || gi_depend == 60)
							li_retfun = FN_RcValidaDistN( gi_depend, "OPL", gi_orden, 
								&gi_ordend ); 
						else
							li_retfun = FN_RcValidaDist( gi_depend, "OPL", gi_orden, 
								&gi_ordend ); 
					}
					else  
						li_retfun = FN_RcValidaDist( gi_depend, gs_tiporec, gi_orden, 
							&gi_ordend );

					if (li_retfun <= 0)
					{
						at_col(rep1,5,"Error : al validar distribucion" );
						print_line(rep1);
						abort_trans(RECDB);
						set_ret_val(-1);
						return(-1);
					}
				}

				if (gi_ordend > 0 && li_distribucion == 1) {
					sprintf(gc_stmsql,"MULTISELECT(%d) ocnumord, ocdepend \
FROM tdbordendc \
WHERE ocdepend = (SELECT MAX(detcentra) FROM tgndepende WHERE dedepend = %d) \
AND ocnumord IN (SELECT pnordend FROM trcpacklisn WHERE pnordenrec = %d AND pntiporec = '%s' AND pndepend = %d) \
UNION \
SELECT ocnumord, ocdepend \
FROM tdbordendc \
WHERE ocdepend = (SELECT MAX(detcentra) FROM tgndepende WHERE dedepend = %d) \
AND octipo = 'P' \
AND ocestado = 'D' \
AND ocnumord = %d ",
						MAXREGS,
						gi_depend,
						gi_orden, gs_tiporec, gi_depend,
						gi_depend, gi_ordend);
				
					gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
				
					if (gi_error_sql <= 0 ) {
						DB_check_error( gi_error_sql);
					}
					li_ordenes = gi_error_sql;
					for(li_idx = 0; li_idx < li_ordenes; li_idx++)
					{
						DB_int_get_field(li_idx*2+1, &gprordendc[li_idx].ocnumord);
						DB_int_get_field(li_idx*2+2, &gprordendc[li_idx].ocdepend);
					}
				
					for(li_idx = 0; li_idx < li_ordenes; li_idx++)
					{
						/** Emitir Listado De O.D. JML!2000/06/13**/
						sprintf(gc_stmsql,"%s.%d", gs_nomrep, gprordendc[li_idx].ocnumord);
						str_put_field(gc_stmsql);
						int_put_field(gprordendc[li_idx].ocdepend);
						int_put_field(gi_depend);
						int_put_field(gprordendc[li_idx].ocnumord);
						int_put_field(0);
						if (strcmp(gs_tiporec,"IMP") == 0 ) 
							int_put_field(1);
						else
							int_put_field(0);
					
						CallService("RRcRepDesLoc");
					}
				}

			} // LOGFIRE
		}

    } /** JML! fin ciclo recibos por viaje **/
    if (strcmp(gs_tiporec, "EM") == 0)
    {
    	if (RecibidoEnCita() < 0)
    	{
	    	at_col(rep1,5,"Error : al actualizar la cita con las unidades recibidas." );
	    	print_line(rep1);
	    	abort_trans(RECDB);
	    	set_ret_val(-1);
	    	return(-1);
    	}
    }
    stcajas.crcodbarr[0] = '\0';
	
	
	
	sprintf(ls_msgAux,"TIPO RECEP : %s",gs_tiporec);
	WriteDebugMsg(ls_msgAux);
	
	if (strcmp(gs_tiporec, "IMP") == 0)
	{
		do
		{
			WriteDebugMsg("Linea 3417 previo consulta");
			
			sprintf(gc_stmsql, "SELECT rocodcaja, COUNT(*), CAST(MIN(plarticul) AS INT), \
CAST(nvl((SELECT MIN(accolor) FROM tcmartcurva WHERE acplu = roplu), 0) AS INT), \
MIN(nvl(SiS(EQUAL(TRIM(rotipo), ''), 'N', rotipo), 'N')) \
FROM trcrecopl, tmpplus \
WHERE roorden = %d AND rodepend = %d AND rocodcaja > '%s' AND rounidad > 0 \
AND UPPER(roplanchado) = 'N' AND plplu = roplu \
AND rocodcaja NOT IN (SELECT Z.rocodcaja FROM trcrecopl Z, tmpnaartplu, tgntiposdf \
WHERE Z.roorden = %d AND Z.rodepend = %d AND Z.roplu = paplu AND panivel01 = ticodigo AND titipo = %d) \
GROUP BY rocodcaja, 4 \
ORDER BY 1",
				gi_orden, gi_depend, stcajas.crcodbarr,
				gi_orden, gi_depend, TIPO_AREA_SIN_CAJA);
			if ((li_cajas = DB_exec_sql(RECDB, gc_stmsql)) < 0) {
				at_col(rep1,5,"Error : al intentar crear las cajas unicas." );
				print_line(rep1);
				abort_trans(RECDB);
				set_ret_val(-1);
				return(-1);
			}
			else if (li_cajas > 0) {
				DB_str_get_field(1, stcajas.crcodbarr);
				if (DB_int_get_field(3, &iArt) < 0) iArt = 0;
				if (DB_int_get_field(4, &iColor) < 0) iColor = 0;
				if (DB_char_get_field(5, &cBultoTarea) < 0) cBultoTarea = 'N';
				sprintf(gc_stmsql, "Paso1 Caja '%s' Articulo %d Color %d BultoTarea '%c'",
						stcajas.crcodbarr, iArt, iColor, cBultoTarea);
				WriteDebugMsg(gc_stmsql);
				
				WriteDebugMsg("FN_CargarContenido LLAMADA");
				if (FN_CargarContenido(stcajas.crcodbarr, gi_depend, gi_orden) > 0)
				{	
					WriteDebugMsg("FN_CargarContenido FIN");
					FN_UbicarCaja(stcajas.crcodbarr, gi_depend, ls_ubica, ls_ubica, 0);
				}
				if (cBultoTarea == 'S') {
					sprintf(gc_stmsql,"SELECT CAST(rounidad/acminimo AS INT) \
FROM trcrecopl, tcmartcurva \
WHERE rodepend = %d AND roorden = %d AND rocodcaja = '%s' \
AND acplu = roplu \
AND acminimo != 0",					
							gi_depend, gi_orden, stcajas.crcodbarr);
					gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
					if (gi_error_sql > 0) {
						if (DB_int_get_field(1, &iTareas) < 0) iTareas = 0;
						sprintf(gc_stmsql, "Paso2 Caja '%s' Articulo %d Color %d BultoTarea '%c' Tareas %d",
								stcajas.crcodbarr, iArt, iColor, cBultoTarea);
						WriteDebugMsg(gc_stmsql);
						sprintf(gc_stmsql,"UPDATE tincajarec \
SET crbultot = 'S', crcolor = %d, crtareas = %d, crarticul = %d \
WHERE crcodbarr = '%s'",
								iColor, iTareas, iArt,
								stcajas.crcodbarr);
						gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
					}
				}
			}
			else if (li_cajas == 0) {
				WriteDebugMsg("Devolvio 0 cajas no llamo a FN_CargarContenido ");
			}
		} while(li_cajas > 0);
	}
	else
	{
		strcpy(stcajas.crcodbarr, "");
		do
		{
			// Dejar hasta que se implemente campo en maestra de proveedor para
			// definir quien trabaja con la recepción por EAN 128 para generar cajas únicas
			
			sprintf(gc_stmsql, "SELECT pnbulto, ChgNullI(pnordend, 0) \
FROM trcpacklisn \
WHERE pnbulto IS NOT NULL AND TRIM(pnbulto) NOT IN ('', '0') \
AND pnordenrec = %d \
AND (pndepend = %d OR %d IN (SELECT dedepend FROM tgndepende WHERE declase = 'B' AND detipodep = 5 AND dezona = 2)) \
AND pntiporec = '%s' AND pnbulto > '%s' \
AND (pnbulto NOT IN (SELECT Z.pnbulto FROM trcpacklisn Z, tmpnaartplu WHERE paplu = pnplu \
AND pnordenrec = %d AND pntiporec = '%s' AND pndepend = %d \
AND panivel01 IN (SELECT ticodigo FROM tgntiposdf WHERE titipo = %d AND ticodigo > 0)) \
OR nvl(pnordend, 0) != 0) \
GROUP BY pnbulto, 2 HAVING SUM(pncant) > 0 ORDER BY pnbulto ",
					gi_orden, gi_depend, gi_depend,
					gs_tiporec, stcajas.crcodbarr,
					gi_orden, gs_tiporec, gi_depend,
					TIPO_AREA_SIN_CAJA);
			
			if ((li_cajas = DB_exec_sql(RECDB, gc_stmsql)) > 0)
			{
				if (DB_str_get_field(1, stcajas.crcodbarr) < 0) strcpy(stcajas.crcodbarr, "");
				if (DB_int_get_field(2, &stcajas.crnumord) < 0) stcajas.crnumord = 0;
				if (strlen(stcajas.crcodbarr) > 0)
				{
					if (FN_CargarContenido(stcajas.crcodbarr, gi_depend, gi_orden) > 0)
					{
						if (stcajas.crnumord == 0)
							FN_UbicarCaja(stcajas.crcodbarr, gi_depend, ls_ubica, ls_ubica, 0);
						else
							FN_DistribuirCaja(stcajas.crcodbarr, gi_depend, stcajas.crnumord);
					}
				}
			}
		} while(li_cajas > 0);
	}
	
	iSecuen = 0;
	strcpy(sPallet, "");
	sprintf(gc_stmsql,"SELECT TRIM(cppallet), CAST(cpconsec AS INT) \
FROM trccarpeta \
WHERE cpdepend = %d AND cpnrorec = %d AND cptiporec = '%s'",
			gi_depend, gi_orden, gs_tiporec);
	gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
	if ( gi_error_sql > 0 ) {
		DB_str_get_field(1, sPallet);
		DB_int_get_field(2, &iSecuen);
		sprintf(gc_stmsql,"UPDATE tincajarec SET crbultot = 'S', \
crcolor = nvl((SELECT MAX(cdcolor) FROM trccarpdet WHERE cdcaja = crcodbarr AND cdconsec = %d), 0), \
crtareas = nvl((SELECT MIN(cdtareas) FROM trccarpdet WHERE cdcaja = crcodbarr AND cdconsec = %d), 0) \
WHERE crcodbarr IN (SELECT cdcaja FROM trccarpdet WHERE cdconsec = %d AND cdtareas > 0)",
				iSecuen,
				iSecuen,
				iSecuen);
		gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
	}
	if (strlen(sPallet) > 1) {
		sprintf(gc_stmsql,"SELECT UNIQUE 1 \
FROM trccarpdet, tinplbulto \
WHERE cdconsec = %d AND pbcodbarr = cdcaja \
UNION \
SELECT UNIQUE 2 \
FROM trccarpdet, tincajarec \
WHERE cdconsec = %d AND crcodbarr = cdcaja",
				iSecuen,
				iSecuen);
		gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
		if (gi_error_sql == 1) {
			DB_int_get_field(1, &iTipo);

			sprintf(gc_stmsql,"DELETE FROM tinpallet WHERE pdpallet = '%s'",
					sPallet);
			gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
			
			sprintf(gc_stmsql,"DELETE FROM tinpalletbult WHERE pbpallet = '%s'",
					sPallet);
			gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
			
			if (iTipo == 1)
				sprintf(gc_stmsql,"INSERT INTO tinpallet(pdpallet, pddepend, pdubica, pdestado, pdfecha, pdviaje, pddepsol, pdusuario, time_stmp) \
SELECT cppallet, cpdepend, '', 'C', '', 0, 0, '', '%s' \
FROM trccarpeta, tinplbulto, trccarpdet \
WHERE cpconsec = %d AND pbcodbarr = cdcaja AND cdconsec = cpconsec \
GROUP BY cppallet, cpdepend",
						get_curr_time(),
						iSecuen);
			else
				sprintf(gc_stmsql,"INSERT INTO tinpallet(pdpallet, pddepend, pdubica, pdestado, pdfecha, pdviaje, pddepsol, pdusuario, time_stmp) \
SELECT cppallet, cpdepend, MIN(crubica), 'C', '', 0, MIN(crdepsol), '', '%s' \
FROM trccarpeta, tincajarec, trccarpdet \
WHERE cpconsec = %d AND crcodbarr = cdcaja AND cdconsec = cpconsec \
GROUP BY cppallet, cpdepend",
						get_curr_time(),
						iSecuen);
			gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
			
			if (iTipo == 1)
				sprintf(gc_stmsql,"INSERT INTO tinpalletbult(pbpallet, pbbulto, time_stmp) \
SELECT UNIQUE '%s', cdcaja, '%s' \
FROM trccarpdet, tinplbulto \
WHERE pbcodbarr = cdcaja AND cdconsec = %d",
						sPallet, get_curr_time(),
						iSecuen);
			else
				sprintf(gc_stmsql,"INSERT INTO tinpalletbult(pbpallet, pbbulto, time_stmp) \
SELECT UNIQUE '%s', cdcaja, '%s' \
FROM trccarpdet, tincajarec \
WHERE crcodbarr = cdcaja AND cdconsec = %d",
						sPallet, get_curr_time(),
						iSecuen);
			gi_error_sql = DB_exec_sql(RECDB,gc_stmsql);
		}
		
	}
    if (gc_swrec != 'V') {
        sprintf(gc_stmsql,"%s",gs_nomrep);
        str_put_field(gc_stmsql);
        int_put_field(gi_depend);
        str_put_field(gs_tiporec);
        int_put_field(gi_orden);
		
        CallService("RRcEntMer");
        str_get_field(1,mensaje);
    }
	
	
	
	
		WriteDebugMsg("Enviando FN_DistPorRecep ");
		int num ; 
		num = FN_DistPorRecep(gi_orden);		
		WriteDebugMsg("Finalizado FN_DistPorRecep. ");		
	
	
    end_trans(RECDB); 
	//end_trans(CLIDB);
	
    EndPacket(gi_PacketID);
	
	
	
	
	if (strcmp(gs_tiporec,"EM") == 0 )
	{
			if(gprparamet->papais == CHILE) {
			   WriteDebugMsg("*** PASO CHILE ****");
			   FN_CmODToXML(gi_depend, gi_num_oc, gi_orden); 
			}
			else {
			   WriteDebugMsg("PASO COLOMBIA");
			   //solo llamar y evaluar
			   /*if (FN_ValidaRecB2B(gi_orden,gi_depend,gs_tiporec) > 0)*/
			   WriteDebugMsg(gs_tiporec);
			   if (FN_ValidaRecB2B(gi_orden,gi_depend,gs_tiporec) > 0)
					FN_RcEmiteRecAdv(gi_orden);
			}
			//imprime reporte//		
			sprintf( gc_stmsql,"CONTSELECT dctipdoc,dcdocum,dcmontobruto, \
dcfemision,dcformato,dctipotrans, \
dcimpuesto,dccodmoneda,dccodcausal,dcobservaciones \
FROM trcdocprov WHERE dcdepend = %d \
AND dcorden = %d AND dctipo = 'EM' \
ORDER BY dcdocum",gi_depend,gi_orden);
			gi_error_sql = DB_exec_sql (RECDB, gc_stmsql);
		   
			if (gi_error_sql < 0 )
			{
				DB_check_error( gi_error_sql);
				return;
			}
			
			//IMPRESION ENCABEZADO//
				
					sprintf(dummy_str,"T");
					at_col(rep1,2,dummy_str); 
									
					sprintf(dummy_str,"Nro.Doc");
					at_col(rep1,4,dummy_str);  
									  
					sprintf(dummy_str,"M.Bruto");
					at_col(rep1,14,dummy_str);
									  
					sprintf(dummy_str,"Emision");
					at_col(rep1,24,dummy_str);
									  
					sprintf(dummy_str,"Formato");
					at_col(rep1,34,dummy_str);
									  
					sprintf(dummy_str,"Transac.");
					at_col(rep1,44,dummy_str);
									  
					sprintf(dummy_str,"Impuesto");
					at_col(rep1,54,dummy_str);
									  
					sprintf(dummy_str,"Moneda");
					at_col(rep1,64,dummy_str);
					 
					sprintf(dummy_str,"Causal");
					at_col(rep1,74,dummy_str);
					  
					sprintf(dummy_str,"Observacion");
					at_col(rep1,84,dummy_str);		
							
					skip_lines(rep1,1);
					sprintf(dummy_str,"----------------------------------------------------------------------------------------------------");
					at_col(report1,2,dummy_str);
					print_line(report1);
					skip_lines(rep1,1);
					
			while (gi_error_sql > 0) 
			{	
				
				   DB_char_get_field(1,&gc_dctipdoc);
				   DB_int_get_field(2, &gi_dcdocum);
				   DB_dbl_get_field(3, &gd_dcmontobruto); 
				   DB_str_get_field(4, gc_dcfemision);
				   DB_str_get_field(5, gc_dcformato);			
				   DB_char_get_field(6, &gc_dctipotrans);
				   DB_int_get_field(7, &gi_dcimpuesto);
				   DB_int_get_field(8, &gi_dccodmoneda); 
				   DB_str_get_field(9, gc_dccodcausal);
				   DB_str_get_field(10, gc_dcobservaciones);
					
					sprintf(dummy_str,"%c", gc_dctipdoc);
					at_col(rep1,2,dummy_str); 
									
					sprintf(dummy_str,"%d", gi_dcdocum);
					at_col(rep1,4,dummy_str);  
									  
					sprintf(dummy_str,"%.0f", gd_dcmontobruto);
					at_col(rep1,14,dummy_str);
									  
					sprintf(dummy_str,"%s", gc_dcfemision);
					at_col(rep1,24,dummy_str);
									  
					sprintf(dummy_str,"%s", gc_dcformato);
					at_col(rep1,34,dummy_str);
									  
					sprintf(dummy_str,"%c", gc_dctipotrans);
					at_col(rep1,44,dummy_str);
									  
					sprintf(dummy_str,"%d", gi_dcimpuesto);
					at_col(rep1,54,dummy_str);
									  
					sprintf(dummy_str,"%d", gi_dccodmoneda);
					at_col(rep1,64,dummy_str);
					 
					sprintf(dummy_str,"%s", gc_dccodcausal);
					at_col(rep1,74,dummy_str);
					  
					sprintf(dummy_str,"%s", gc_dcobservaciones);
					at_col(rep1,84,dummy_str);		
		
					
				
				print_line(rep1);
					  
				gi_error_sql = DB_exec_sql( RECDB, NULL );
				
				if (gi_error_sql < 0)
				 {
					DB_check_error(gi_error_sql);
					set_ret_val(-1);				
					print_line(rep1);
					return;
				 }
				
        }			
	}
	
    set_ret_val(0);
    return(1);
}


/************************************************************************
* int RRcAprRec()                                                       *
*                                                                       *
* Proposito: Administra las diferencias en despachos.                   *
*                                                                       *   
* Observaciones:                                                        *  
*                                                                       * 
* Autor: FAM                                                            *
*                                                                       *
* Fecha: 30-Ene-95                                                      *
*                                                                       * 
************************************************************************/
int RRcAprRec()
{
    int li_ind, iReturn;

        gprvalper =(struct tinvalper *)malloc(sizeof(struct tinvalper));
        gprinventa =(struct tinventa *)malloc(sizeof(struct tinventa));
        gprestprov =(struct tcmestprov *)malloc(sizeof(struct tcmestprov));
        gprrecdet =(struct trcrecdet *)malloc(sizeof(struct trcrecdet));
        gprtrans =(struct tintrans *)malloc(sizeof(struct tintrans));
        gprparamet=(struct tgnparamet *)malloc(sizeof(struct tgnparamet));

        for (li_ind = 0; li_ind < 200; li_ind++) 
     gprrecibos[li_ind] =(struct trcrecibos *)malloc(sizeof(struct trcrecibos));
  
    gi_error_sql = PR_Rcaprobar();
    if (gi_error_sql < 0) {
		set_ret_val(gi_error_sql);
        at_col(rep1,5,"Por favor intente de nuevo la Aprobacion");
        print_line(rep1);
		iReturn = get_ret_val();
		//abort_trans(CLIDB);
		set_ret_val(iReturn);
    }
    else if (gi_error_sql == 0) {
		set_ret_val(get_ret_val());
    }

	
    end_report(rep1); 
    return(get_ret_val());
}

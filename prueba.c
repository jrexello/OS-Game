#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <SDL/SDL.h> 
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>
#include "colisiones.h"

//ctes de las se�ales
#define SIGMOVHEROE SIGUSR1
#define SIG 52

#define TRUE 1
#define FALSE 0

//dimensiones pantalla
#define ANCHO_PANTALLA 640
#define ALTO_PANTALLA 480
#define AVANCE_HEROE 5

#define ANCHO_HEROE 50
#define ALTO_HEROE 80
#define POS_X_HEROE_INI (ANCHO_PANTALLA)/2-ANCHO_HEROE/2
#define POS_Y_HEROE_INI ALTO_PANTALLA - ALTO_HEROE - 5
#define FINAL_JUEGO 0
#define INMUNE 1
#define CARGANDO 2
#define INTERVALO 3
#define ALIENS 4
#define PUNTOS 5
#define ANCHO_ALIEN 69
#define ALTO_ALIEN 108
#define NUMEROALIENS 20
#define ANCHO_SHOT 13
#define ALTO_SHOT 44
#define NUMERODISPAROS 20

key_t clave;
long int iden;
int *zmem = NULL;



//Variables memoria compartida Aliens
//Creamos la estructura del alien

typedef struct eAlienDef {
    int PID;
    int x;
    int y;
} eAlien;

key_t claveAlien;
long int identiAlien;
eAlien *zAlien = NULL;

//Para los disparos

typedef struct eDisparoDef {
    int PID;
    int x;
    int y;
} eDisparo;
eDisparo cargador[NUMERODISPAROS]; //Array en donde guardaremos los disparos

typedef u_char bool;

// Para compilar:
//  gcc `sdl-config --cflags` -c prueba.c
//  gcc -o p2 prueba.o `sdl-config --libs` -lSDL_image

//	VARIABLES GLOBALES

SDL_Surface *ttexto;
TTF_Font *fuente;
char msg[11];
SDL_Color bgcolor, fgcolor;
SDL_Color blanco;

SDL_Surface *pantalla, *fondo, *Heroe, *Heroei, *Alien, *Disparo, *Heroehit;
SDL_Rect rect;
Uint32 color;
Uint8 *keys;
SDL_TimerID timer;

SDL_Event evento;
pid_t PID_Ppal, PID_Heroe, PID_Alien, PID_Shot, PID_Inm, PID_Conf;
bool terminar = FALSE;
bool sal = FALSE; //Lo usamos para saber cuando debe terminar de cargar el archivo de configuración.
int pos_x_heroe, pos_y_heroe, pos_x_alien, pos_y_alien;
int phx_ant, phy_ant;
int numero_aliens; //Número total que habrá de aliens
int interval; //Tiempo entre la aparición de los aliens.
int puntos; //Puntuación actual.




//tuberias de posiciones
int tuberia_pos_x_heroe[2];
int tuberia_pos_y_heroe[2];
int tuberia_tecla_heroe[2];
int tuberia_disparos[2];

int coord_iniciales = TRUE;
int NumVidas = 3;
int tiempo;
int tecla;
int num_aliens;

////	DECLARACI�N DE FUNCIONES	///////////////////////////////

//Carga, liberacion de imagenes
int CargarImagenes();
void liberar();

//Pintar elementos en pantalla
void pintarfondo();
void PintaHeroe(int pos_x, int pos_y);
void pintaAlien(int pos_x, int pos_y);
void pintaTodosAliens();
void pintaDisparo(int pos_x, int pos_y);
void pintaTodosDisparos();
//configuracion para capturar se�ales
void SenialMovimientoHeroe();
void Senialteclaheroe();

//manejadores de se�ales
void tuberiaHeroe();
void teclaheroe();

//Funciones asociadas a procesos
void Movimientoheroe(int x, int y);
void buscaAlien();
void creaAlien(int i);
void creaDisparo();

//Timer
Uint32 Funcion_Callback(Uint32 intervalo, void *param);

//Pintar Texto en pantalla
void pintatexto(int posX, int posY, char *tipo, int tamanio, SDL_Color fuente);


///////////////////////////  FUNCIONES      ///////////////////

char * situacionActual(){
    char texto[255];
    int uno =1;
    int dos=2;
    int tres=3;
    sprintf(texto, "Vidas: %d Puntuación restante: %d Aliens restantes: %d", uno, dos, tres);
    //printf("%s\n", texto);
    return texto;
}


void actualizaCargador() {
    read(tuberia_disparos[0], &cargador, sizeof cargador);
}

void actualizaPipe() {
    write(tuberia_disparos[1], &cargador, sizeof cargador);
}

void buscaDisparo() {
    //Buscamos una posición libre
    int i;
    bool encontrado = FALSE;
    //read(tuberia_disparos[0], &cargador, sizeof cargador);
    //close(tuberia_disparos[0]);
    //printf("PreAct\n");
    //printf("PostActBusca\n");
    for (i = 0; i < NUMERODISPAROS; i++) {
        //printf("Analizando posición %d, PID es %d\n", i, cargador[i].PID);
        if ((cargador[i].PID == 0) && !encontrado) {
            //printf("Posición libre encontrada en %d\n", i);
            creaDisparo(i);
            encontrado = TRUE;
        }
    }

    //write(tuberia_disparos[1], &cargador, (sizeof (struct eDisparoDef)) * NUMERODISPAROS);
}

void buscaAlien() {
    //Buscamos una posición libre
    int i;
    bool encontrado = FALSE;
    for (i = 0; i < NUMEROALIENS; i++) {
        if ((zAlien[i].PID == 0) && !encontrado) {
            //printf("Posición libre encontrada en %d\n", i);
            creaAlien(i);
            encontrado = TRUE;
        } else if (zAlien[i].PID != 0) {
            zAlien[i].y += 10;
            if (zAlien[i].y >= pos_y_heroe){
                //Alien toca el suelo
                zAlien[i].PID = 0;
                puntos -= 5;
            }
                
        }
    }
}

Uint32 Funcion_Callback(Uint32 intervalo, void *param) {
    //printf("New Timer\n");
    buscaAlien();
    usleep(100);
    buscaAlien();
    SDL_RemoveTimer(timer);
    timer = SDL_AddTimer(((Uint32)(interval*1000)), Funcion_Callback, pantalla);
    return 0;
}

void SenialMovimientoHeroe() {
    struct sigaction act;
    act.sa_handler = tuberiaHeroe;
    act.sa_flags = SA_RESTART;
    sigemptyset(&act.sa_mask);
    sigaction(SIGMOVHEROE, &act, NULL);
}

void Senialteclaheroe() {
    struct sigaction act;
    act.sa_handler = teclaheroe;
    act.sa_flags = SA_RESTART;
    sigemptyset(&act.sa_mask);
    sigaction(SIG, &act, NULL);
}

void tuberiaHeroe() {
    phx_ant = pos_x_heroe;
    phy_ant = pos_y_heroe;
    read(tuberia_pos_x_heroe[0], &pos_x_heroe, sizeof (pos_x_heroe));
    read(tuberia_pos_y_heroe[0], &pos_y_heroe, sizeof (pos_y_heroe));
    actualizaCargador();
    actualizaPipe();
}

void teclaheroe() {
    read(tuberia_tecla_heroe[0], &tecla, 1);
}

/* Funcion asociada al proceso heroe, controla todos sus movimiento y las acciones que debe llevar a cabo 
 * dependiendo de la tecla que haya sido pulsada y que le haya enviado el proceso principal */

void MovimientoHeroe() {
    while (!zmem[FINAL_JUEGO]) {
        if (tecla != 0) {
            if (tecla == 1) {
                pos_x_heroe -= AVANCE_HEROE;
                if (pos_x_heroe < 0) pos_x_heroe = 0;
            } else if (tecla == 2) {
                pos_x_heroe += AVANCE_HEROE;
                if (pos_x_heroe + ANCHO_HEROE > ANCHO_PANTALLA)
                    pos_x_heroe = ANCHO_PANTALLA - ANCHO_HEROE;
            } else if (tecla == 3) {
                //Para que nos aseguremos de que la tecla no se queda pulsada, enviamos a un hijo a hacer la operación.
                //actualizaPipe();
                pid_t hijo = fork();
                if (hijo == 0) {
                    //printf("A buscar un dioosparo!\n");
                    actualizaCargador();
                    buscaDisparo();
                    actualizaPipe();
                    //close(tuberia_disparos[0]);
                    //close(tuberia_disparos[1]);
                    exit(2);
                } else {//PP
                    waitpid(hijo, NULL, 0); //Esperamos a que termine el hijo
                    actualizaCargador();
                    actualizaPipe();
                }
            }
            write(tuberia_pos_y_heroe[1], &pos_y_heroe, sizeof (pos_y_heroe));
            write(tuberia_pos_x_heroe[1], &pos_x_heroe, sizeof (pos_x_heroe));
            kill(getppid(), SIGMOVHEROE);
            tecla = 0;

        }
        SDL_Delay(100);
    }
}

/* Funcion que es llamada por el proceso principal para comprobar que tecla ha sido pulsada 
 * por los jugadores y enviarla al proceso protagonista que corresponda */

void tecla_pulsada() {
    while (SDL_PollEvent(&evento)) {
        if (evento.type == SDL_QUIT)
            zmem[FINAL_JUEGO] = TRUE;
        if (evento.type == SDL_KEYDOWN) {
            if (evento.key.keysym.sym == SDLK_ESCAPE)
                zmem[FINAL_JUEGO] = TRUE;
            if (evento.key.keysym.sym == SDLK_LEFT)
                tecla = 1;
            if (evento.key.keysym.sym == SDLK_RIGHT)
                tecla = 2;
            if (evento.key.keysym.sym == SDLK_RETURN)
                tecla = 3;
        }
        if (tecla != 0) {
            write(tuberia_tecla_heroe[1], &tecla, 1);
            SDL_Delay(50);
            kill(PID_Heroe, SIG);
            tecla = 0;
        }
    }

}

/* Programa principal */

int main(int argc, char *argv[]) {
    srand(time(NULL)); //Para los números Random.

    printf("Bienvenido a esta especie de Space Commander.\nEn primer lugar, dime ¿Prefieres jugar con Cumfito[o] o con Cunfita[a]?\n\t[o/a]>>");
    char pj = getchar();
    if ((pj != 'a') && (pj != 'o')) {
        printf("Eso no es ni a ni o. En fín, te pondré a cumfito\n");

    } else if (pj == 'a') {
        printf("Has elegido a Cumfita. Girl power!\nSuerte en la batalla\n");

    }

    phx_ant = POS_X_HEROE_INI;
    phy_ant = POS_Y_HEROE_INI;
    pos_x_heroe = POS_X_HEROE_INI;
    pos_y_heroe = POS_Y_HEROE_INI;
    pos_x_alien = 20;
    pos_y_alien = 30;
    tiempo = SDL_GetTicks();



    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1) //Inicializa SDL con los subsistemas a inicializar
    {
        printf("No se pudo iniciar el video: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    // Establecimiento de modo de video, devuelve un puntero a una superficie SDL, donde se 
    // ubican los gr�ficos del juego. 
    // SDL_HWSURFACE: Crea la superficie de video en memoria principal
    // SDL_SWSURFACE: Crea la superficie de video en memoria de video
    // SDL_DOUBLEBUF: Permite utilizar doble buffer. Solo se puede utilizar con SDL_HWSURFACE

    pantalla = SDL_SetVideoMode(ANCHO_PANTALLA, ALTO_PANTALLA, 24, SDL_HWSURFACE | SDL_DOUBLEBUF);
    if (!pantalla) {
        printf("No se pudo iniciar el modo de pantalla: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }
    // Inicializamos SDL_ttf
    if (TTF_Init() < 0) {
        printf("No se pudo iniciar SDL_ttf: %s\n", SDL_GetError());
        return 1;
    }

    if (CargarImagenes() == -1) {
        SDL_Quit();
        exit(1);
    }
    //preparamos la pantalla
    pintarfondo();
    //printf("Fondo pintado!\n");
    //Creamos las tuberias de comunicacion entre procesos
    pipe(tuberia_pos_x_heroe); // Tuberia para comunicar la posici�n x del heroe
    pipe(tuberia_pos_y_heroe); // Tuberia para comunicar la posicion y del heroe
    pipe(tuberia_disparos);
    pipe(tuberia_tecla_heroe);
    //Rellenamos la pipe de los disparos
    int i;
    for (i = 0; i < NUMERODISPAROS; i++) {
        cargador[i].PID = 0;
        cargador[i].x = 0;
        cargador[i].y = 0;
    }
    write(tuberia_disparos[1], &cargador, sizeof cargador);
    //close(tuberia_disparos[1]);
    //close(tuberia_disparos[1]);
    //Comprobamos que se ha escrito bien;
    //read(tuberia_disparos[0], &cargador, (sizeof (struct eDisparoDef))*NUMERODISPAROS);
    //close(tuberia_disparos[0]);
    /*for (i = 0; i < NUMERODISPAROS; i++) {
        printf("Disparo %d con valor %d\n", i, cargador[i].PID);
    }*/
    // Creamos memoria compartida para comunicacion entre procesos
    clave = ftok("/bin/ls", 33);
    iden = shmget(clave, sizeof (int)*10, 0777 | IPC_CREAT);
    zmem = (int *) shmat(iden, (char *) 0, 0);
    //Memoria compartida para final Alien

    claveAlien = ftok("/bin/ls", 13);
    identiAlien = shmget(claveAlien, sizeof (eAlien) * NUMEROALIENS, 0777 | IPC_CREAT);
    zAlien = (eAlien *) shmat(identiAlien, (char *) 0, 0);
    for (i = 0; i < NUMEROALIENS; i++) {
        zAlien[i].PID = 0;
        //printf("Posición %d con PID %d\n",i,zAlien[i].PID);
    }

    zmem[CARGANDO] = 1; //No dejemos que se ejecuitte el juego hasta que hayamos terminado de cargar el archivo de configuración.

    //Ponemos las configuración en default:
    interval = 2;
    numero_aliens = 150;
            
    //Leemos el fichero de configuración;
    PID_Conf = fork();
    if (PID_Conf == 0) { //Es el hijo
        //Abrimos el fichero de configuración:
        FILE *fconf;
        fconf = fopen("practica.conf", "r"); //Lo abrimos solo para lectura
        char buff[255]; //Aquí guardamos los strings que usemos.
        char* numero;
        while (!feof(fconf)) {
            fgets(buff, 255, (FILE*) fconf);
            //Leemos una línea
            char* token = strtok(buff, " ");
            if (strncmp("numero_alienigenas", token, 18) == 0) {
                // Estamos en la line de alienígenas
                numero = strtok(NULL, " "); //Cojemos el número detrás del espacio
                int numAlien = (int) strtol(numero, (char **)NULL, 10);//Lo convertimos a int
                if((numAlien > 200)||(numAlien<100)){
                    printf("Error al cargar el archivo de configuiración. Número de Aliens incorrecto\n");
                    zmem[FINAL_JUEGO] = TRUE;
                    exit(0);
                }else{
                    //Lo carga correctamente
                    printf("Leemos el número de alienígenas: ");
                    numero_aliens = numAlien;
                    printf("%d\n", numero_aliens);
                }
                } //Fin if numero_alienigenas
            
            else if(strncmp("tiempo_intervalo", token, 18) == 0){
                //Estamos en tiempo intervalo
                printf("Leemos intervalo: ");
                numero = strtok(NULL, " "); //Cojemos el número detrás del espacio
                int interv = (int) strtol(numero, (char **)NULL, 10);//Lo convertimos a int
                if((interv < 1) || (interv > 3)){
                    //Error
                    printf("Error al cargar el archivo de configuiración. tiempo de intervalo incorrecto\n");
                    zmem[FINAL_JUEGO] = TRUE;
                    exit(0);
                }else{
                    //Es correcto                  
                    interval = interv;
                    printf("%d\n", interval);
                }
            } //Fin if intervalo
            
            else if(strncmp("puntuacion", token, 10) == 0){
                //estamos en puntuación
                printf("Leemos la puntuación inicial: ");
                numero = strtok(NULL, " "); //Cojemos el número detrás del espacio
                int puntIni = (int) strtol(numero, (char **)NULL, 10); //Convertido a int
                if((puntIni<10 ) || (puntIni>20)){
                    //Error
                    printf("Error al cargar el archivo de configuiración. Puntuación inicial Inválida\n");
                    zmem[FINAL_JUEGO] = TRUE;
                    exit(0);
                }else{
                    //Es correcto
                    puntos = puntIni;
                    printf("%d\n", puntos);
                }
            }
            }
        //Volcamos los datos a memoria compartida...
        zmem[PUNTOS] = puntos;
        zmem[ALIENS] = numero_aliens;
        zmem[INTERVALO] = interval; 
        fclose(fconf);

        //Permitimos al padre continuar
        printf("Archivo de configuración cargado correctamente\n");
        zmem[CARGANDO] = 0;
        exit(0);

    } else if (PID_Conf > 0) {//Padre

        while (zmem[CARGANDO] == TRUE) { //Lo paramos hasta que reciba la señal del hijo.
            //Do Nothing           
        }
        
        //Volcamos los datos:
        interval = zmem[INTERVALO];
        numero_aliens = zmem[ALIENS];
        puntos = zmem[PUNTOS];
    }

    // Comienza el juego con la creacion de procesos 
    PID_Heroe = fork();
    if (PID_Heroe == 0) {
        Senialteclaheroe();
        MovimientoHeroe();
        exit(1);
    } else //PROCESO PRICIPAL
    {

        SenialMovimientoHeroe();
        SDL_Delay(150);
        timer = SDL_AddTimer((Uint32)(interval*1000), Funcion_Callback, pantalla);
        while (!zmem[FINAL_JUEGO]) {
            //actualizaPipe();
            tecla_pulsada();
            pintarfondo();
            pintatexto(10, 50, "./fonts/fuente1.ttf", 14, blanco);
            PintaHeroe(pos_x_heroe, pos_y_heroe);
            pintaTodosAliens();
            //actualizaCargador();
            //actualizaPipe();
            pintaTodosDisparos();
            SDL_Flip(pantalla);
            SDL_Delay(30);

        }
    }
    SDL_Delay(200);
    TTF_Quit();
    SDL_Quit();
    liberar();
    shmdt((char *) zmem);
    shmdt((eAlien *) zAlien);
    shmctl(iden, IPC_RMID, 0);
    shmctl(identiAlien, IPC_RMID, 0);
    exit(0);
}

/* Libera las imagenes cargadas de memoria */
void liberar() {
    SDL_FreeSurface(fondo);
    SDL_FreeSurface(Heroe);
    SDL_FreeSurface(Heroei);
    SDL_FreeSurface(Heroehit);
    SDL_FreeSurface(Alien);
    SDL_FreeSurface(Disparo);
}

/* Carga las imagenes necesarias para el juego en memoria, esta carga se podria hacer dinamica
 * de tal manera que ocuparamos la menor cantidad de memoria posible */

int CargarImagenes() {
    blanco.r = 255;
    blanco.g = 255;
    blanco.b = 255;
    fondo = SDL_LoadBMP("Images/fondo2.bmp"); // Carga la imagen pasada como parametro 
    if (!fondo) { // y la guarda en la variable declarada como SDL_Surface.
        printf("No se pudo cargar fondo\n");
        return (-1);
    }
    color = SDL_MapRGB(fondo->format, 0, 0, 0); // Permite tomar el color de la imagen que queremos 
    // hacer transparente
    SDL_SetColorKey(fondo, SDL_SRCCOLORKEY | SDL_RLEACCEL, color); //Establece el color deseado
    //como transparente

    Heroe = SDL_LoadBMP("Images/camina1.bmp");
    if (!Heroe) {
        printf("No se pudo cargar imagen del heroe\n");
        return (-1);
    }
    SDL_SetColorKey(Heroe, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(Heroe->format, 255, 255, 255));
    Heroei = SDL_LoadBMP("Images/caminai1.bmp");
    if (!Heroei) {
        printf("No se pudo cargar imagen del heroe\n");
        return (-1);
    }
    SDL_SetColorKey(Heroei, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(Heroei->format, 255, 255, 255));

    Heroehit = SDL_LoadBMP("Images/hit.bmp");
    if (!Heroehit) {
        printf("No se pudo cargar imagen del heroe\n");
        return (-1);
    }
    SDL_SetColorKey(Heroehit, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(Heroei->format, 255, 255, 255));

    //Alien
    Alien = SDL_LoadBMP("Images/salta.bmp");
    if (!Alien) {
        printf("No se pudo cargar al alien");
        return (-1);
    }
    SDL_SetColorKey(Alien, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(Alien->format, 255, 255, 255));

    //Disparo
    Disparo = SDL_LoadBMP("Images/DISPARO.BMP");
    if (!Disparo) {
        printf("No se ha podido cargar el disparo");
        return (-1);
    }
    return (0);
}

/* Dibuja la imagen establecida como fondo en la superficie del juego */
void pintarfondo() {
    // SDL_Rect establece un area rectangular
    rect = (SDL_Rect){0, 0, ANCHO_PANTALLA, ALTO_PANTALLA};
    //Dibuja el area rectangular establecida anteriormente en la superficie indicada
    SDL_FillRect(pantalla, &rect, color);
    rect = (SDL_Rect){40, 60, ANCHO_PANTALLA, ALTO_PANTALLA};
    //Copia zonas rectangulares de una superficie a otra
    SDL_BlitSurface(fondo, NULL, pantalla, &rect);
}

/* Dibuja el personaje protagonista */
void PintaHeroe(int pos_x, int pos_y) {
    rect = (SDL_Rect){pos_x, pos_y, ANCHO_HEROE, ALTO_HEROE};
    if (zmem[INMUNE] == 1) {
        SDL_BlitSurface(Heroehit, NULL, pantalla, &rect);
        //Le pintamos mientras recibe el daño.
    }
    else if (phx_ant > pos_x)
        SDL_BlitSurface(Heroei, NULL, pantalla, &rect);
    else
        SDL_BlitSurface(Heroe, NULL, pantalla, &rect);
}

void pintaTodosAliens() {
    //Recorremos la lista de aliens.
    int i;
    for (i = 0; i < NUMEROALIENS; i++) {
        if (zAlien[i].PID != 0) {
            //printf("En la posición %d hay un alien.\n", i);
            pintaAlien(zAlien[i].x, zAlien[i].y);
            int j;
            eDisparo dispAux;
            for (j = 0; j < NUMERODISPAROS; j++) {
                dispAux = cargador[j];

                if (SDL_Collide(Disparo, cargador[j].x, cargador[j].y, Alien, zAlien[i].x, zAlien[i].y)) {
                    zAlien[i].PID = 0;
                    numero_aliens--;
                    cargador[j].PID =0;
                    if(numero_aliens == 0){
                        printf("Victoria!!!!\n");
                        zmem[FINAL_JUEGO] = 1;
                    }
                }
            }
            if (SDL_Collide(Heroe, pos_x_heroe, pos_y_heroe, Alien, zAlien[i].x, zAlien[i].y)) {
                if (zmem[INMUNE] == 0) {
                    zAlien[i].PID = 0;
                    zAlien[i].x = pos_x_alien;
                    zAlien[i].y = pos_y_alien;
                    NumVidas--;
                    printf("Solo te quedan %d vidas\n", NumVidas);
                    if (NumVidas == 0) {
                        printf("Fin del Juego\n");
                        zmem[FINAL_JUEGO] = TRUE;
                    } else {
                        //Le hacemos inmune 2 segundos
                        PID_Inm = fork();
                        if (PID_Inm == 0) {
                            zmem[INMUNE] = 1;
                            sleep(2);
                            zmem[INMUNE] = 0;
                            exit(0);
                        }
                    }
                }
            }
        }
    }
}

void pintaAlien(int pos_x, int pos_y) {
    rect = (SDL_Rect){pos_x, pos_y, ANCHO_ALIEN, ALTO_ALIEN};
    if (Alien != NULL)
        SDL_BlitSurface(Alien, NULL, pantalla, &rect);
}

void pintaTodosDisparos() {
    int i;
    //printf("preAct\n");
    //printf("PostAct\n");
    for (i = 0; i < NUMERODISPAROS; i++) {
        //printf("Disparo con PID = %d \n", cargador[i].PID);
        if (cargador[i].PID != 0) {
            //printf("Pintado disparo %d siendo el número %d\n", cargador[i].PID, i);
            pintaDisparo(cargador[i].x, cargador[i].y);
            cargador[i].y -= 10;
            //Comprobamos si ha chocado con algún alien:
            int j;
            eAlien alienAux;
            for (j = 0; j < NUMEROALIENS; j++) {
                alienAux = zAlien[j];

                if (SDL_Collide(Disparo, cargador[i].x, cargador[i].y, Alien, alienAux.x, alienAux.y)) {
                    cargador[i].PID = 0;
                } else if (cargador[i].y <= pos_y_heroe - ALTO_PANTALLA) {
                    cargador[i].PID = 0;
                }
            }

        }
    }
    actualizaPipe();
    actualizaCargador();
    //printf("Preparado para pintar\n");
    //close(tuberia_disparos[0]);

}

void pintaDisparo(int pos_x, int pos_y) {
    rect = (SDL_Rect){pos_x, pos_y, ANCHO_SHOT, ALTO_SHOT};

    if (Disparo != NULL)
        SDL_BlitSurface(Disparo, NULL, pantalla, &rect);
}

void pintatexto(int posX, int posY, char *tipo, int tamanio, SDL_Color fuente) {

    SDL_Surface *ttext;
    SDL_Rect rectangulo;
    SDL_Color fondo;
    TTF_Font *font;
    font = TTF_OpenFont(tipo, tamanio);    
    char msg[100];
    sprintf(msg, "Vidas: %d Puntuacion restante: %d Aliens restantes: %d", NumVidas, puntos, numero_aliens);
    fondo.r = 38;
    fondo.g = 36;
    fondo.b = 79;
    ttext = TTF_RenderText_Shaded(font, msg, fuente, fondo);
    if (!ttext) {

        printf("Error al cargar el texto %s\n", msg);
        SDL_Quit();
        exit(-1);
    }
    rectangulo.x = posX;
    rectangulo.y = posY;
    rectangulo.w = ttext->w;
    rectangulo.h = ttext->h;


    SDL_SetColorKey(ttext, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(ttext->format, fondo.r, fondo.g, fondo.b));
    SDL_BlitSurface(ttext, NULL, pantalla, &rectangulo);
    TTF_CloseFont(font);
}

void creaAlien(int pos) {
    PID_Alien = fork();
    int off; //Diferencia con la posición actual.
    off = rand() % 460;
    if (PID_Alien == 0) {

        zAlien[pos].PID = getpid();
        zAlien[pos].x = pos_x_alien + off;
        zAlien[pos].y = pos_y_alien;
        //printf("Alien con pid %d creado en posición %d.\n", zAlien[pos].PID, pos);
        exit(1);
    }
}

void creaDisparo(int pos) {
    //printf("Creando disparo\n");
    //Crea el disparo
    //eDisparo cargadorAux[NUMERODISPAROS];
    /*
            int i;
            for(i=0; i<NUMERODISPAROS; i++){
                cargadorAux[i].PID=0;
                cargadorAux[i].x=0;
                cargadorAux[i].y=0;
            }
     */
    //printf("PreRead\n");
    //read(tuberia_disparos[0], &cargadorAux, sizeof cargadorAux);

    //printf("Cargadoraux creado\n");
    cargador[pos].PID = getpid();
    cargador[pos].x = pos_x_heroe;
    cargador[pos].y = pos_y_heroe;

    //write(tuberia_disparos[1], &cargador, sizeof cargador);



}


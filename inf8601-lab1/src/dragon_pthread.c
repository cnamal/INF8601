/*
 * dragon_pthread.c
 *
 *  Created on: 2011-08-17
 *      Author: Francis Giraldeau <francis.giraldeau@gmail.com>
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#include "dragon.h"
#include "color.h"
#include "dragon_pthread.h"

pthread_mutex_t mutex_stdout;

void printf_threadsafe(char *format, ...) {
    va_list ap;

    va_start(ap, format);
    pthread_mutex_lock(&mutex_stdout);
    vprintf(format, ap);
    pthread_mutex_unlock(&mutex_stdout);
    va_end(ap);
}

void *dragon_draw_worker(void *data) {
    /* 1. Initialiser la surface */
    /* 2. Dessiner le dragon */
    /* 3. Effectuer le rendu final */
    return NULL;
}

int dragon_draw_pthread(char **canvas, struct rgb *image, int width, int height, uint64_t size, int nb_thread) {
    TODO("dragon_draw_pthread");

    pthread_t *threads = NULL;
    pthread_barrier_t barrier;
    limits_t lim;
    struct draw_data info;
    char *dragon = NULL;
    int i;
    int scale_x;
    int scale_y;
    struct draw_data *data = NULL;
    struct palette *palette = NULL;
    int ret = 0;
    int r;

    palette = init_palette(nb_thread);
    if (palette == NULL)
        goto err;

    /* 1. Initialiser barrier. */

    if (dragon_limits_pthread(&lim, size, nb_thread) < 0)
        goto err;

    info.dragon_width = lim.maximums.x - lim.minimums.x;
    info.dragon_height = lim.maximums.y - lim.minimums.y;

    if ((dragon = (char *) malloc(info.dragon_width * info.dragon_height)) == NULL) {
        printf("malloc error dragon\n");
        goto err;
    }

    if ((data = malloc(sizeof(struct draw_data) * nb_thread)) == NULL) {
        printf("malloc error data\n");
        goto err;
    }

    if ((threads = malloc(sizeof(pthread_t) * nb_thread)) == NULL) {
        printf("malloc error threads\n");
        goto err;
    }

    info.image_height = height;
    info.image_width = width;
    scale_x = info.dragon_width / width + 1;
    scale_y = info.dragon_height / height + 1;
    info.scale = (scale_x > scale_y ? scale_x : scale_y);
    info.deltaJ = (info.scale * width - info.dragon_width) / 2;
    info.deltaI = (info.scale * height - info.dragon_height) / 2;
    info.nb_thread = nb_thread;
    info.dragon = dragon;
    info.image = image;
    info.size = size;
    info.limits = lim;
    info.barrier = &barrier;
    info.palette = palette;
    info.dragon = dragon;
    info.image = image;

    /* 2. Lancement du calcul parallèle principal avec draw_dragon_worker */

    /* 3. Attendre la fin du traitement */

    /* 4. Destruction des variables (à compléter). */

done:
    FREE(data);
    FREE(threads);
    free_palette(palette);
    *canvas = dragon;
    return ret;

err:
    FREE(dragon);
    ret = -1;
    goto done;
}

void *dragon_limit_worker(void *data) {
    struct limit_data *lim = (struct limit_data *) data;
    piece_limit(lim->start, lim->end, &lim->piece);
    return NULL;
}

void swapSorting(int64_t *x1, int64_t *x2){
    if(*x1 > *x2)
    {
        int64_t temp = *x1;
        *x1 = *x2;
        *x2 = temp;
    }

}

void rotate(bool direction,piece_t* piece, xy_t *initial){
    if(direction)
    {
        //left direction
        rotate_left(&piece->limits.minimums);
        rotate_left(&piece->limits.maximums);
        rotate_left(&piece->orientation);
        rotate_left(&piece->position);
        swapSorting(&piece->limits.minimums.x,&piece->limits.maximums.x);
        swapSorting(&piece->limits.minimums.y,&piece->limits.maximums.y);
        //mise a jour de l'orientation
        rotate_left(initial);
    }else{
        //right direction
        rotate_right(&piece->limits.minimums);
        rotate_right(&piece->limits.maximums);
        rotate_right(&piece->orientation);
        rotate_right(&piece->position);
        swapSorting(&piece->limits.minimums.x,&piece->limits.maximums.x);
        swapSorting(&piece->limits.minimums.y,&piece->limits.maximums.y);
        //mise a jour de l'orientation
        rotate_right(initial);
    }
}


/*
 * Calcule les limites en terme de largeur et de hauteur de
 * la forme du dragon. Requis pour allouer la matrice de dessin.
 * size represente le nombre d'iteration en puissance de 2 pour la creation de la fractal
 */
int dragon_limits_pthread(limits_t *limits, uint64_t size, int nb_thread) {

    int ret = 0;
    int i;
    pthread_t *threads = NULL;
    struct limit_data *thread_data = NULL;
    piece_t master;

    piece_init(&master);

    /*
     * On pourrait optimiser le code dans le cas où size > nb_thread
     * en assignant size à nb_thread. Les variables n'étant pas de même types
     * nous avons choisi de ne pas faire cette optimisation.
     */
    
    /* 1. ALlouer de l'espace pour threads et threads_data. */
    threads = malloc(sizeof(pthread_t)*nb_thread);
    thread_data = malloc(sizeof(struct limit_data)*nb_thread);

    /* 2. Lancement du calcul en parallèle avec dragon_limit_worker. */
    int remaining_threads = nb_thread;
    uint64_t remaining_size = size;
    uint64_t start = 0;
    for(i=0;i<nb_thread;i++){
        uint64_t thread_size = remaining_size/remaining_threads;
        uint64_t end=start+thread_size;
        thread_data[i].start=start;
        thread_data[i].end=end;
        thread_data[i].piece.orientation.x = 1;
        thread_data[i].piece.orientation.y = 1;
        start = end;
        remaining_size-=thread_size;
        remaining_threads--;
    }

    for(i=0;i<nb_thread;i++){
        int res = pthread_create(threads+i,NULL,dragon_limit_worker,thread_data+i);
        if(res){
            //TODO
            // Join previous created threads if any
            // goto err
        }
    }

    /* 3. Attendre la fin du traitement. */
    for(i=0;i<nb_thread;i++){
        int res = pthread_join(threads[i],NULL);
        if(res){
            //TODO
            // goto err
        }
    }
    //fusion des resultats
    master.limits=thread_data[0].piece.limits;
    for(i=0;i<nb_thread-1;i++){
        uint64_t skipped = thread_data[i].end+1;
        xy_t last_orientation = thread_data[i].piece.orientation;
        //choix du sens de la rotation (limité le nombre maximal de rotation a 2)
        bool left = true;
        if(last_orientation.x==1 && last_orientation.y==-1)
            left = false;
        //orientation par default
        xy_t initial = {1,1};
         while(!equal_orientation(initial,last_orientation)){
             rotate(left,&thread_data[i+1].piece,&initial);
         }


        //translate
        thread_data[i+1].piece.position.x+=thread_data[i].piece.position.x;
        thread_data[i+1].piece.position.y+=thread_data[i].piece.position.y;
        thread_data[i+1].piece.limits.minimums.x+=thread_data[i].piece.position.x;
        thread_data[i+1].piece.limits.minimums.y+=thread_data[i].piece.position.y;
        thread_data[i+1].piece.limits.maximums.x+=thread_data[i].piece.position.x;
        thread_data[i+1].piece.limits.maximums.y+=thread_data[i].piece.position.y;

        if(thread_data[i+1].piece.limits.minimums.x < master.limits.minimums.x)
            master.limits.minimums.x=thread_data[i+1].piece.limits.minimums.x;

        if(thread_data[i+1].piece.limits.minimums.y < master.limits.minimums.y)
            master.limits.minimums.y=thread_data[i+1].piece.limits.minimums.y;

        if(master.limits.maximums.x < thread_data[i+1].piece.limits.maximums.x)
            master.limits.maximums.x=thread_data[i+1].piece.limits.maximums.x;

        if(master.limits.maximums.y < thread_data[i+1].piece.limits.maximums.y)
            master.limits.maximums.y=thread_data[i+1].piece.limits.maximums.y;
    }


done:
    FREE(threads);
    FREE(thread_data);
    *limits = master.limits;
    return ret;

err:
    ret = -1;
    goto done;
}

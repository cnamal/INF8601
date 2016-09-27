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
    struct draw_data *info = (struct draw_data *)data;
    /* 1. Initialiser la surface */
    int64_t area = info->dragon_height*info->dragon_width;
    uint64_t start = info->id * info->size / info->nb_thread;
    uint64_t end = (info->id + 1) * info->size / info->nb_thread;
    init_canvas(info->id*area/info->nb_thread, (info->id+1)*area/info->nb_thread, info->dragon, -1);

    pthread_barrier_wait(info->barrier);

    /* 2. Dessiner le dragon */
    dragon_draw_raw(start,end,info->dragon,info->dragon_width,info->dragon_height,info->limits,info->id);
    pthread_barrier_wait(info->barrier);

    /* 3. Effectuer le rendu final */
    scale_dragon(info->id*info->image_height/info->nb_thread,(info->id+1)*info->image_height/info->nb_thread,info->image,info->image_width,info->image_height,info->dragon,info->dragon_width,info->dragon_height,info->palette);

    return NULL;
}

int dragon_draw_pthread(char **canvas, struct rgb *image, int width, int height, uint64_t size, int nb_thread) {

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
    r = pthread_barrier_init(&barrier,NULL,nb_thread);
    if(r)
        goto err;

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
    for(i=0;i<nb_thread;i++){
        data[i]=info;
        data[i].id=i;
        r=pthread_create(threads+i,NULL,dragon_draw_worker,data+i);
        if(r){
            int j;
            for(j=0;j<i;j++)
                pthread_join(threads[i],NULL);
            goto err;
        }
    }

    /* 3. Attendre la fin du traitement */
    r=0;
    for(i=0;i<nb_thread;i++)
        r |= pthread_join(threads[i],NULL);

    if(r)
        goto err;

    /* 4. Destruction des variables (à compléter). */
    r=pthread_barrier_destroy(&barrier);
    if(r)
        goto err;

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

    /* 1. ALlouer de l'espace pour threads et threads_data. */
    threads = malloc(sizeof(pthread_t)*nb_thread);
    if(threads==NULL)
        goto err;

    thread_data = malloc(sizeof(struct limit_data)*nb_thread);
    if(thread_data==NULL)
        goto err;

    /* 2. Lancement du calcul en parallèle avec dragon_limit_worker. */
    int remaining_threads = nb_thread;
    uint64_t remaining_size = size;
    uint64_t start = 0;
    for(i=0;i<nb_thread;i++){
        uint64_t thread_size = remaining_size/remaining_threads;
        uint64_t end=start+thread_size;
        thread_data[i].start=start;
        thread_data[i].end=end;
        thread_data[i].piece=master;
        start = end;
        remaining_size-=thread_size;
        remaining_threads--;
    }

    for(i=0;i<nb_thread;i++){
        int res = pthread_create(threads+i,NULL,dragon_limit_worker,thread_data+i);
        if(res){
            int j;
            for(j=0;j<i;j++)
                pthread_join(threads[i],NULL);
            goto err;
        }
    }

    /* 3. Attendre la fin du traitement. */
    int res=0;
    for(i=0;i<nb_thread;i++)
        res |= pthread_join(threads[i],NULL);


    if(res)
        goto err;

    //fusion des resultats
    master=thread_data[0].piece;

    for(i=1;i<nb_thread;i++)
        piece_merge(&(master),thread_data[i].piece);


done:
    FREE(threads);
    FREE(thread_data);
    *limits = master.limits;
    return ret;

err:
    ret = -1;
    goto done;
}

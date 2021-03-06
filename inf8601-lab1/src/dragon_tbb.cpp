/*
 * dragon_tbb.c
 *
 *  Created on: 2011-08-17
 *      Author: Francis Giraldeau <francis.giraldeau@gmail.com>
 */

#include <iostream>

extern "C" {
#include "dragon.h"
#include "color.h"
#include "utils.h"
}

#include "dragon_tbb.h"
#include "tbb/tbb.h"
#include "TidMap.h"

#define STATS 0

using namespace std;
using namespace tbb;
class DragonLimits {
    piece_t piece;
public:
    DragonLimits(){
        piece_init(&piece);
    }

    DragonLimits(DragonLimits& d,split ){
        piece_init(&piece);
    }

    void operator()(const blocked_range<uint64_t>& range){
        uint64_t start = range.begin();
        uint64_t end = range.end();
        piece_limit(start, end, &piece);
    }

    void join(DragonLimits& rhs){
        piece_merge(&piece,rhs.piece);
    }

    piece_t getPiece(){
        return piece;
    }
};

class DragonDraw {
    struct draw_data data;
    TidMap* tidMap;
public:
    DragonDraw(struct draw_data& data,TidMap* tidMap):data(data),tidMap(tidMap){

    }
    void operator()(const blocked_range<uint64_t >& r)const{

#if STATS
	int pos;
	pos = tidMap->getIdFromTid(gettid());
	if(pos >= 0)
		data.tid[pos]++;
#endif

        if(data.size< static_cast<uint64_t>(data.nb_thread)){
            for(uint64_t i=r.begin()*static_cast<uint64_t>(data.nb_thread)/data.size;i<r.end()*static_cast<uint64_t>(data.nb_thread)/data.size;i++){
                uint64_t start = i * data.size / data.nb_thread;
                uint64_t end = (i + 1) * data.size / data.nb_thread;
                if(start==end)
                    continue;
                if(end>r.end())
                    break;
                dragon_draw_raw(start, end, data.dragon, data.dragon_width, data.dragon_height, data.limits, i);
		
            }
        }else{
            uint64_t start = r.begin();
            int idStart =start * data.nb_thread/data.size;
            bool br=false;
            while(true){
                int idEnd = idStart+1;
                uint64_t end = idEnd*data.size/data.nb_thread;
		
                if(r.end()<=end){
                    end=r.end();
                    br = true;
                }
                dragon_draw_raw(start,end,data.dragon,data.dragon_width,data.dragon_height,data.limits,idStart);
                if(br)
                    break;
                start =end;
                idStart++;

            }
        }
    }
};

class DragonRender {
    struct draw_data data;

public:
    DragonRender(struct draw_data& data):data(data){

    }
    void operator()(const blocked_range<int>& r)const{
        int start = r.begin();
        int end = r.end();
        scale_dragon(start,end,data.image,data.image_width,data.image_height,data.dragon,data.dragon_width,data.dragon_height,data.palette);
    }
};

class DragonClear {
    char *dragon;
public:
    DragonClear(char *dragon):dragon(dragon){

    }
    void operator()(const blocked_range<int>& r ) const{
        init_canvas(r.begin(),r.end(),dragon,-1);
    }

};

int dragon_draw_tbb(char **canvas, struct rgb *image, int width, int height, uint64_t size, int nb_thread) {
    
    TidMap tidMap(nb_thread);
    struct draw_data data;
    limits_t limits;
    char *dragon = NULL;
    int dragon_width;
    int dragon_height;
    int dragon_surface;
    int scale_x;
    int scale_y;
    int scale;
    int deltaJ;
    int deltaI;
    
    struct palette *palette = init_palette(nb_thread);
    if (palette == NULL)
        return -1;

    /* 1. Calculer les limites du dragon */
    dragon_limits_tbb(&limits, size, nb_thread);

    task_scheduler_init init(nb_thread);

    dragon_width = limits.maximums.x - limits.minimums.x;
    dragon_height = limits.maximums.y - limits.minimums.y;
    dragon_surface = dragon_width * dragon_height;
    scale_x = dragon_width / width + 1;
    scale_y = dragon_height / height + 1;
    scale = (scale_x > scale_y ? scale_x : scale_y);
    deltaJ = (scale * width - dragon_width) / 2;
    deltaI = (scale * height - dragon_height) / 2;

    dragon = (char *) malloc(dragon_surface);
    if (dragon == NULL) {
        free_palette(palette);
        return -1;
    }

    data.nb_thread = nb_thread;
    data.dragon = dragon;
    data.image = image;
    data.size = size;
    data.image_height = height;
    data.image_width = width;
    data.dragon_width = dragon_width;
    data.dragon_height = dragon_height;
    data.limits = limits;
    data.scale = scale;
    data.deltaI = deltaI;
    data.deltaJ = deltaJ;
    data.palette = palette;
    data.tid = (int *) calloc(nb_thread, sizeof(int));

    /* 2. Initialiser la surface : DragonClear */
    parallel_for(blocked_range<int>(0,dragon_width*dragon_height),DragonClear{dragon});

    /* 3. Dessiner le dragon : DragonDraw */
    parallel_for(blocked_range<uint64_t>(0,size),DragonDraw{data,&tidMap});
#if STATS
    tidMap.dump();
    cout << "-------------" << endl;
    int k;
    int somme = 0;
    for(k=0;k<nb_thread;k++){
	somme+= data.tid[k];
    }
    cout << somme << endl;
#endif

    /* 4. Effectuer le rendu final */
    parallel_for(blocked_range<int>(0,height),DragonRender{data});

    init.terminate();

    free_palette(palette);
    FREE(data.tid);
    *canvas = dragon;
    return 0;
}

/*
 * Calcule les limites en terme de largeur et de hauteur de
 * la forme du dragon. Requis pour allouer la matrice de dessin.
 */
int dragon_limits_tbb(limits_t *limits, uint64_t size, int nb_thread) {

    DragonLimits lim;
    piece_t piece;
    piece_init(&piece);
    task_scheduler_init init(nb_thread);
    parallel_reduce(blocked_range<uint64_t>(0,size),lim);
    init.terminate();
    *limits = lim.getPiece().limits;
    return 0;
}

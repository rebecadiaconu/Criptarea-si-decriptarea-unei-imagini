#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct{
    unsigned char pR;
    unsigned char pG;
    unsigned char pB;
}pixel;

typedef struct {
    int ind1, ind2;
    unsigned int latime, inaltime;
    int numar;
    double corr;
} templat;

void citireDimensiuni(char* nume_fisier_sursa, unsigned int *latime, unsigned int *inaltime) {

    unsigned int latimeIMG, inaltimeIMG;
    FILE *f = fopen(nume_fisier_sursa, "rb");
    if (f == NULL) printf("Nu am putut deschide fisierul %s. \n", nume_fisier_sursa);
    else {
        fseek(f, 18, 0);
        fread(&latimeIMG, sizeof(unsigned int), 1, f);
        fread(&inaltimeIMG, sizeof(unsigned int), 1, f);

        *latime = latimeIMG;
        *inaltime = inaltimeIMG;
    }
    fclose(f);
}

void liniarizare(pixel **Liniar, char* nume_fis_sursa)
{
    unsigned int latime, inaltime;
    unsigned char pRGB[3];
    FILE *f = fopen(nume_fis_sursa, "rb");
    if(f == NULL) printf("Nu am putut deschide fisierul %s . \n", nume_fis_sursa);
    else
    {
        printf("Numele fisierului sursa este: %s \n", nume_fis_sursa);
        citireDimensiuni(nume_fis_sursa, &latime, &inaltime);
        printf("Latimea si inaltimea imaginii sunt: %u %u \n", latime, inaltime);

        // calculam padding-ul imaginii
        int padding;
        if(latime % 4 == 0) padding=0;
        else
            padding = 4 - 3*latime%4;

        fseek(f, 54, 0);
        int i, j;

        // retinem imaginea sub forma unui tablou unidimensional, parcursa de sus in jos
        for(i=inaltime-1; i>=0;i--)
        {
            for(j=0; j<latime; j++)
            {
                fread(pRGB,3, 1, f);
                (*Liniar)[i*latime + j].pB = pRGB[0];
                (*Liniar)[i*latime + j].pG = pRGB[1];
                (*Liniar)[i*latime + j].pR = pRGB[2];
            }
            fseek(f,padding,1);
        }
        fclose(f);
    }

}

void xorShift32(char *nume_fis_seed, unsigned int **R , unsigned int latime, unsigned int inaltime)
{
    unsigned int r0;
    int k, i;
    k = 2*latime*inaltime;
    FILE *text = fopen(nume_fis_seed, "r");
    if(text == NULL) printf("Nu am putut deschide fisierul text.\n");
    else fscanf(text, "%u", &r0);

    *(R)[0]=r0;

    // generez 2 * latime_img * inaltime_img - 1 numere random, folosind algoritmul xorshift32
    for(i=1; i<k; i++)
    {
        r0 = r0 ^ r0 << 13;
        r0 = r0 ^ r0 >> 17;
        r0 = r0 ^ r0 << 5;
        (*R)[i] = r0;
    }
    fclose(text);
}

void permutariCriptare(pixel **Perm, pixel *Liniar, unsigned int *R, unsigned int latime, unsigned int inaltime)
{
    int i, aux, j, k, *p;
    unsigned r;

    p = (int*)malloc(inaltime*latime*sizeof(int));
    if(p == NULL) printf("Nu am putut aloca memorie. \n");

    for(i=0; i<latime*inaltime; i++)
        p[i] = i;

    // folosind primele latime_img * inaltime_img numere random, generez permutarile pe care le voi folosi la criptare
    for(k=inaltime*latime-1; k>=1; k--)
    {
        r = R[inaltime*latime-k]%(k+1);
        aux = p[r];
        p[r] = p[k];
        p[k] = aux;
    }

    // permut pixelii imaginii in functie de permutarile obtinute
    for(i=0; i<inaltime; i++)
        for(j=0; j<latime; j++) {
            (*Perm)[p[i * latime + j]] = Liniar[i * latime + j];
        }
    free(p);

}

// functie pe care o folosesc in generarea octetilor unui numar fara semn, pentru functia de criptare a imaginii ( 255 = 2^0 + 2^1 + ... + 2^7)
pixel XORconstanta(unsigned int x)
{
    pixel y;
    unsigned int c;
    c = x & 255;
    y.pB = (unsigned char)c;
    x = x >> 8;
    c = x & 255;
    y.pG = (unsigned char)c;
    x = x >> 8;
    c = x & 255;
    y.pR = (unsigned char)c;

    return y;
}

void afisare(pixel *Liniar, unsigned int inaltime, unsigned int latime, char* nume_destinatie, char* fisier_sursa)
{
    int i, j, padding, k;
    unsigned char pRGB[3], header[54];

    FILE *fout = fopen(nume_destinatie, "wb");
    FILE *fin = fopen(fisier_sursa, "rb");
    if(fin == NULL) printf("Nu am putut deschide fisierul .\n");
    if(fout == NULL) printf("Nu am putut deschide fisierul .\n");

    // calculez padding-ul imaginii
    if(latime % 4 != 0)
        padding = 4 - (3 * latime) % 4;
    else
        padding = 0;

    // citesc header-ul din imaginea initiala si il copiez in imaginea formata
    fread(header, 54, 1, fin);
    fwrite(header, 54, 1, fout);

    for(i=inaltime-1; i>=0; i--)
    {
        for(j=0; j<latime; j++)
        {
            pRGB[0]=Liniar[i*latime+j].pB;
            pRGB[1]=Liniar[i*latime+j].pG;
            pRGB[2]=Liniar[i*latime+j].pR;
            fwrite(pRGB,3,1,fout);
        }
        // adaug padding-ul in imaginea formata
        k=0;
        while(k<padding)
        {
            unsigned char c='0';
            fwrite(&c,1,1,fout);
            k++;
        }
    }

    fclose(fout);
    fclose(fin);

}

void criptareImag(char *fisier_sursa, char *fisier_destinatie, char *text)
{
    pixel *ImagLin, *C, *PermC;
    unsigned int sv, *R, latime, inaltime;
    int i;

    citireDimensiuni(fisier_sursa, &latime, &inaltime);

    R = (unsigned int*)malloc(2*latime*inaltime*sizeof(unsigned int));
    ImagLin = (pixel*)malloc(inaltime*latime*sizeof(pixel));
    C = (pixel*)malloc(inaltime*latime*sizeof(pixel));
    PermC = (pixel*)malloc(inaltime*latime*sizeof(pixel));

    if(R == NULL) printf("Nu am putut aloca memorie. \n");
    if(ImagLin == NULL) printf("Nu am putut aloca memorie. \n");
    if(C == NULL) printf("Nu am putut aloca memorie. \n");
    if(PermC == NULL) printf("Nu am putut aloca memorie. \n");

    liniarizare(&ImagLin, fisier_sursa);
    xorShift32(text, &R, latime, inaltime);
    permutariCriptare(&PermC, ImagLin, R, latime, inaltime);

    FILE *f = fopen(text, "r");
    // citesc cheia secreta din fisierul text
    while(feof(f) == 0)
    {
        fscanf(f, "%u", &sv);
        if(feof(f)) break;
    }

    // ma folosesc de functia XORconstanta(x) pentru a realiza criptarea imaginii,
    // mai intai pentru pozitia 0 si apoi pentru restul imaginii, in forma liniarizata
    pixel a, b;
    a = XORconstanta(sv);
    b = XORconstanta(R[inaltime*latime]);
    C[0].pB = PermC[0].pB ^ a.pB ^ b.pB;
    C[0].pG = PermC[0].pG ^ a.pG ^ b.pG;
    C[0].pR = PermC[0].pR ^ a.pR ^ b.pR;

    for(i=1; i<latime*inaltime; i++)
    {
        pixel a = XORconstanta(R[inaltime*latime+i]);
        C[i].pB = PermC[i].pB ^ C[i-1].pB ^ a.pB;
        C[i].pG = PermC[i].pG ^ C[i-1].pG ^ a.pG;
        C[i].pR = PermC[i].pR ^ C[i-1].pR ^ a.pR;
    }

    // creez o noua imagine in format bitmap dupa criptare
    afisare(C,inaltime, latime, fisier_destinatie,fisier_sursa);

    free(R);
    free(PermC);
    free(ImagLin);
    free(C);

}

//functie folosita pentru decriptarea imaginii, unde salvez in forma liniarizata imaginea pe care doresc sa o decriptez,
// conform inversarii regulii de substitutie
void ImagInterm(char *fisier_text,pixel *C, pixel **Interm, unsigned int *R, unsigned int latime, unsigned int inaltime)
{
    unsigned int sv;
    int i;

    FILE *text = fopen(fisier_text, "r");
    while(feof(text) == 0)
    {
        fscanf(text, "%u", &sv);
        if(feof(text)) break;
    }

    pixel a, b;
    a = XORconstanta(sv);
    b = XORconstanta(R[inaltime*latime]);
    (*Interm)[0].pB = C[0].pB ^ a.pB ^ b.pB;
    (*Interm)[0].pG = C[0].pG ^ a.pG ^ b.pG;
    (*Interm)[0].pR = C[0].pR ^ a.pR ^ b.pR;

    for(i=1; i<inaltime*latime; i++)
    {
        pixel a = XORconstanta(R[inaltime*latime+i]);
        (*Interm)[i].pB = C[i].pB ^ C[i-1].pB ^ a.pB;
        (*Interm)[i].pG = C[i].pG ^ C[i-1].pG ^ a.pG;
        (*Interm)[i].pR = C[i].pR ^ C[i-1].pR ^ a.pR;
    }

}

// generez inaltime_img * latime_img permutari, dupa care calculez inversele lor si formez o imagine noua, decriptata
void permutariDecriptare(pixel **D, pixel *Interm, unsigned int *R, unsigned int latime, unsigned int inaltime)
{
    int i, j, k, *p, *q;
    unsigned int r, aux;

    p = (int*)malloc(inaltime*latime*sizeof(int));
    q = (int*)malloc(inaltime*latime*sizeof(int));

    for(i=0; i<latime*inaltime; i++)
        p[i] = i;

    for(k=inaltime*latime-1; k>=1; k--)
    {
        r = R[inaltime*latime-k]%(k+1);
        aux = p[r];
        p[r] = p[k];
        p[k] = aux;
    }

    for(i=0; i<inaltime*latime; i++)
        q[p[i]] = i;

    for(i=0; i<inaltime; i++)
        for(j=0; j<latime; j++)
        {
            (*D)[q[i*latime+j]] = Interm[i*latime+j];
        }

    free(p);
    free(q);

}

// apelez functiile prezentate mai sus, pentru realizarea criptarii,
// conform algoritmului dat in enunt
void decriptareImag(char *fisier_sursa, char *fisier_destinatie, char *text) {

    pixel *C, *Interm, *D;
    int i, j, *q, *p;
    unsigned int latime, inaltime, *R;

    citireDimensiuni(fisier_sursa, &latime, &inaltime);

    C = (pixel*)malloc(inaltime*latime*sizeof(pixel));
    Interm = (pixel*)malloc(inaltime*latime*sizeof(pixel));
    R = (unsigned int*)malloc(2*inaltime*latime*sizeof(unsigned int));
    D = (pixel*)malloc(inaltime*latime*sizeof(pixel));

    if(R == NULL) printf("Nu am putut aloca memorie. \n");
    if(D == NULL) printf("Nu am putut aloca memorie. \n");
    if(C == NULL) printf("Nu am putut aloca memorie. \n");
    if(Interm == NULL) printf("Nu am putut aloca memorie. \n");

    liniarizare(&C,fisier_sursa);
    xorShift32(text, &R, latime, inaltime);
    ImagInterm(text, C, &Interm, R, latime, inaltime);
    permutariDecriptare(&D, Interm, R, latime, inaltime);
    afisare(D, inaltime, latime, fisier_destinatie, fisier_sursa);


        free(D);
        free(C);
        free(Interm);
        free(R);

}

void chi(char *fisier_sursa)
{
    int i, j;
    pixel *ImagLin;
    unsigned int *frec_R, *frec_G, *frec_B, latime, inaltime;

    citireDimensiuni(fisier_sursa, &latime, &inaltime);

    // creez 3 vectori de frecventa, cate unul pentru fiecare culoare ( R, G sau B)
    frec_R = (unsigned int*)malloc(256*sizeof(unsigned int));
    frec_G = (unsigned int*)malloc(256*sizeof(unsigned int));
    frec_B = (unsigned int*)malloc(256*sizeof(unsigned int));
    ImagLin = (pixel*)malloc(inaltime*latime*sizeof(pixel));

    if(frec_R == NULL) printf("Nu am putut aloca memorie. \n");
    if(frec_G == NULL) printf("Nu am putut aloca memorie. \n");
    if(frec_B == NULL) printf("Nu am putut aloca memorie. \n");

    // ii initializez cu 0
    for(i=0;i<256;i++)
        frec_R[i] = 0;

    for(i=0;i<256;i++)
        frec_G[i] = 0;

    for(i=0;i<256;i++)
        frec_B[i] = 0;

    liniarizare(&ImagLin, fisier_sursa);

    for(i=0; i<inaltime*latime; i++)
    {
        unsigned x, y, z;
        z = ImagLin[i].pB;
        y = ImagLin[i].pG;
        x = ImagLin[i].pR;
        frec_R[x]++;
        frec_G[y]++;
        frec_B[z]++;
    }

    // calculez pentru fiecare culoare in parte valoarea testului chi-squared
    double sR=0, sG=0, sB=0;
    double fmediu = (latime*inaltime)/256;


    for(i=0; i<256; i++)
    {
        unsigned int fi = frec_R[i];
        sR = sR + (fi*fi - 2*fi*fmediu + fmediu*fmediu)/fmediu;
    }

    for(i=0; i<256; i++)
    {
        unsigned int fi = frec_G[i];
        sG = sG + (fi*fi - 2*fi*fmediu + fmediu*fmediu)/fmediu;
    }

    for(i=0; i<256; i++)
    {
        unsigned int fi = frec_B[i];
        sB = sB + (fi*fi - 2*fi*fmediu + fmediu*fmediu)/fmediu;
    }
    printf("\nRezultatele testului chi pentru %s sunt:\n Pentru canalul R: %.2f \n Pentru canalul G: %.2f \n Pentr canalul B: %.2f. \n", fisier_sursa, sR, sG, sB);

    free(ImagLin);
    free(frec_B);
    free(frec_G);
    free(frec_R);

}


void grayscale_image(char* nume_fisier_sursa,char* nume_fisier_destinatie)
{
    FILE *fin, *fout;
    unsigned int dim_img, latime_img, inaltime_img;
    unsigned char pRGB[3], header[54], aux;

    printf("nume_fisier_sursa = %s \n",nume_fisier_sursa);

    fin = fopen(nume_fisier_sursa, "rb");
    if(fin == NULL)
    {
        printf("Nu am gasit imaginea sursa din care citesc");
        return;
    }

    fout = fopen(nume_fisier_destinatie, "wb+");

    fseek(fin, 2, SEEK_SET);
    fread(&dim_img, sizeof(unsigned int), 1, fin);
    printf("Dimensiunea imaginii in octeti: %u\n", dim_img);

    fseek(fin, 18, SEEK_SET);
    fread(&latime_img, sizeof(unsigned int), 1, fin);
    fread(&inaltime_img, sizeof(unsigned int), 1, fin);
    printf("Dimensiunea imaginii in pixeli (latime x inaltime): %u x %u\n",latime_img, inaltime_img);

    //copiaza octet cu octet imaginea initiala in cea noua
    fseek(fin,0,SEEK_SET);
    unsigned char c;
    while(fread(&c,1,1,fin)==1)
    {
        fwrite(&c,1,1,fout);
        fflush(fout);
    }
    fclose(fin);

    //calculam padding-ul pentru o linie
    int padding;
    if(latime_img % 4 != 0)
        padding = 4 - (3 * latime_img) % 4;
    else
        padding = 0;

    printf("padding = %d \n",padding);

    fseek(fout, 54, SEEK_SET);
    int i,j;
    for(i = 0; i < inaltime_img; i++)
    {
        for(j = 0; j < latime_img; j++)
        {
            //citesc culorile pixelului
            fread(pRGB, 3, 1, fout);
            //fac conversia in pixel gri
            aux = 0.299*pRGB[2] + 0.587*pRGB[1] + 0.114*pRGB[0];
            pRGB[0] = pRGB[1] = pRGB[2] = aux;
            fseek(fout, -3, SEEK_CUR);
            fwrite(pRGB, 3, 1, fout);
            fflush(fout);
        }
        fseek(fout,padding,SEEK_CUR);
    }
    fclose(fout);
}

// functie folosita pentru a calcula intensitatea medie in sablonul glisat pe imagine
double sablonMediu(pixel *sab, unsigned int sabL, unsigned int sabH)
{
    int i, j;
    double sum = 0;

    for(i=0; i<sabH; i++)
        for(j=0; j<sabL; j++)
        {
            sum = sum + sab[i*sabL + j].pR;
        }
    sum = sum/(sabL*sabH);
    return sum;
}

//functie folosita pentru a calcula intensitatea medie in fereastra formata
double imgMediu(pixel *Imag, unsigned int latime, unsigned int sabL, unsigned int sabH, const int a, const int b)
{
    int k, q;
    double sum = 0;

    for(k=a; k<a+sabH; k++)
        for(q=b; q<b+sabL; q++)
        {
            sum = sum + Imag[k*latime + q].pR;
        }
    sum = sum/(sabL*sabH);
    return sum;

}

//functie folosita pentru a calcula deviatia standard in sablonul glisat pe imagine
double sablonDev(pixel *sab, unsigned int sabL, unsigned int sabH)
{
    int i, j;
    double sum = 0, x;

    x = sablonMediu(sab, sabL, sabH);

    for(i=0; i<sabH; i++)
        for(j=0; j<sabL; j++)
        {
            sum = sum + (sab[i*sabL+j].pR - x)*(sab[i*sabL+j].pR - x) ;
        }
    sum = sum/(sabL*sabH-1);
    sum = sqrt(sum);
    return sum;
}

//functie folosita pentru a calcula deviatia standard in fereastra formata
double imgDev(pixel *Imag, unsigned int latime, unsigned int sabL, unsigned int sabH,const int i,const int j)
{
    int k, q;
    double sum = 0, x;

    x = imgMediu(Imag, latime, sabL, sabH, i, j);

    for(k=i; k<i+sabH; k++)
        for(q=j; q<j+sabL; q++)
        {
            sum = sum + (Imag[k*latime+q].pR - x)*(Imag[k*latime+q].pR - x);
        }
    sum = sum/(sabL*sabH-1);
    sum = sqrt(sum);
    return sum;
}


// calculez pentru fiecare fereastra si sablon in parte corelatia, verific daca este peste pragul admis
// in caz afirmativ, retin pozitiile de la care am format imaginea, corelatia acesteia cu sablonul, cifra asociata sablonului
// si dimensiunile ei intr-un vector de tip structura, definita la inceputul programului
void vector(char *nume_sablon, pixel *Imag, unsigned int latime, unsigned int inaltime, templat**D, int *nrElem, int indice, float prag)
{
    pixel *sab;
    unsigned int sabL, sabH, n;
    double Smediu, Fmediu, devS, devF;
    int i, j, k, q, nr;

    citireDimensiuni(nume_sablon, &sabL, &sabH);
    sab = (pixel*)malloc(sabL*sabH*sizeof(pixel));
    if(sab == NULL) printf("Nu am putut aloca memorie. \n");
    liniarizare(&sab, nume_sablon);

    n = sabL*sabH;
    nr = (*nrElem);

    Smediu = sablonMediu(sab, sabL, sabH);
    devS = sablonDev(sab, sabL, sabH);

    for(i=0; i<inaltime && i+sabH<inaltime; i++)
    {
        for(j=0; j<latime && j+sabL<latime; j++)
        {
            double sum = 0;
            Fmediu = imgMediu(Imag,latime,sabL, sabH, i, j);
            devF = imgDev(Imag, latime, sabL, sabH, i, j);

            for(k=i; k<i+sabH; k++)
                for(q=j; q<j+sabL; q++)
                {
                    sum = sum + (Imag[k*latime+q].pR - Fmediu)*(sab[(k-i)*sabL + (q-j)].pR - Smediu);
                }
            sum = sum/(sabL*sabH*devS*devF);
            if(sum >= prag){
                (*D)=(templat*)realloc((*D),(nr+2)*sizeof(templat));

                (*D)[nr].corr = sum;
                (*D)[nr].ind1 = i;
                (*D)[nr].ind2 = j;
                (*D)[nr].numar = indice;
                (*D)[nr].latime = sabL;
                (*D)[nr].inaltime = sabH;

                nr++;
            }
        }
    }
    (*nrElem) = nr;
    free(sab);

}

void contur(pixel *Imag, unsigned int latime, templat a)
{
    int i, j, t, ind1, ind2;
    unsigned int sabL, sabH, x, y, z;

    sabL = a.latime;
    sabH = a.inaltime;
    ind1 = a.ind1;
    ind2 = a.ind2;

    t = a.numar;
    switch(t)
    {
        case 0:
        {
            x = 255;
            y = z = 0;
        } break;
        case 1:
        {
            x = y = 255;
            z = 0;
        } break;
        case 2:
        {
            x = z = 0;
            y = 255;
        } break;
        case 3:
        {
            x = 0;
            y = z = 255;
        } break;
        case 4:
        {
            x = z = 255;
            y = 0;
        } break;
        case 5:
        {
            x = y = 0;
            z = 255;
        } break;
        case 6:
        {
            x = y = z = 192;
        }break;
        case 7:
        {
            x = 255;
            y = 140;
            z = 0;
        } break;
        case 8:
        {
            x = z = 128;
            y = 0;
        } break;
        case 9:
        {
            x = 128;
            y = z = 0;
        }break;
        default:
        {
            printf("Nu exista culoare pentru aceasta alegere. \n");
        }break;
    }

    for(i=ind1; i<ind1+sabH; i++)
    {
        Imag[i*latime+ind2].pR = Imag[i*latime+ind2+sabL-1].pR = (unsigned char)x;
        Imag[i*latime+ind2].pG = Imag[i*latime+ind2+sabL-1].pG = (unsigned char)y;
        Imag[i*latime+ind2].pB = Imag[i*latime+ind2+sabL-1].pB = (unsigned char)z;
    }

    for(j=ind2+1; j<ind2+sabL-1; j++)
    {
        Imag[ind1*latime+j].pR = Imag[(ind1+sabH-1)*latime + j].pR = (unsigned char)x;
        Imag[ind1*latime+j].pG = Imag[(ind1+sabH-1)*latime + j].pG = (unsigned char)y;
        Imag[ind1*latime+j].pB = Imag[(ind1+sabH-1)*latime + j].pB = (unsigned char)z;
    }


}

//functia de comparare folosita la qsort
int compareCorr(const void *a, const void *b)
{
    templat va = *(templat*)a;
    templat vb = *(templat*)b;

    if(va.corr > vb.corr) return -1;
    if(va.corr == vb.corr) return 0;
    return 1;
}

// calculez aria de intersectie a ferestrei cu sablonul
double arie(templat a, templat b)
{
    int x, y;
    x = a.inaltime - fabs(a.ind1 - b.ind1);
    y = a.latime - fabs(a.ind2 - b.ind2);
    return (double)(x*y);
}

double suprapunere(templat a, templat b)
{
    double sup, a1, a2, a3;

    a1 = a.inaltime * a.latime;
    a2 = b.inaltime * b.latime;
    a3 = arie(a,b);
    sup = a3/(a1+a2-a3);

    return sup;
}

// in cazul in care suprapunerea este peste 0.2, sterg pozitia respectiva din vector
void stergere(templat **D, int *nrElem, int indice){
    int i;
    for(i=indice; i<(*nrElem)-1; i++)
        (*D)[i] = (*D)[i+1];
    (*nrElem)--;
}

// verific daca cele 2 suprafete(fereastra formata din punctul (i,j) si sablonul) se intersecteaza
int intersec(templat a, templat b)
{
    if(a.ind1 > b.ind1 + b.inaltime || b.ind1 > a.ind1 + a.inaltime) return 0;
    if(a.ind2 > b.ind2 + b.latime || b.ind2 > a.ind2 + a.latime) return 0;
    return 1;

}

void maxime(templat *D, int *nrElem)
{
    int i,j, nr = (*nrElem);

    // sortez descrescator vectorul in care am salvat detectiile
    qsort(D, (*nrElem), sizeof(templat),compareCorr);


    for(i=0; i<nr-1; i++)
        for(j=i+1; j<nr; j++)
        {
            if (intersec(D[i], D[j])) {
                double sup = suprapunere(D[i], D[j]);
                if (sup > 0.2) {
                    stergere(&D, &nr, j);
                }
            }
        }
    (*nrElem) = nr;

}

void templateMatching(char *imag_init, char *imag_color, char *fisier_text, float prag)
{
    pixel *ImagG, *Imag;
    templat *D = NULL;
    int i, nrElem = 0, numarSab;
    unsigned int latime, inaltime;
    char nume_sablon[101], imaggri[101];

    printf("Numele imaginii %s dupa grayscale este: ", imag_init);
    scanf("%s", imaggri);
    grayscale_image(imag_init,imaggri);

    FILE *text = fopen(fisier_text, "r");
    if(text == NULL) printf("Nu am putut deschide fisierul %s. \n", fisier_text);

    // citesc dimensiunile imaginii grayscale din header-ul ei si o salvez sub forma liniarizata
    citireDimensiuni(imaggri, &latime, &inaltime);
    ImagG = (pixel*)malloc(inaltime*latime*sizeof(pixel));
    if(ImagG == NULL) printf("Nu am putut aloca memmorie pentru imaginea data. \n");
    liniarizare(&ImagG, imaggri);

    printf("Numarul de sabloane cu care lucram este: "); scanf("%d", &numarSab);

    // citesc pe rand numele sabloanelor pe care le glisez peste imagine si formez vectorul de detectii
    for(i=0; i<numarSab; i++)
    {
        fscanf(text, "%s \n", nume_sablon);
        char sablon_gray[101];
        printf("Numele sablonului %s dupa grayscale este: ", nume_sablon);
        scanf("%s", sablon_gray);
        grayscale_image(nume_sablon,sablon_gray);
        vector(sablon_gray, ImagG, latime, inaltime, &D, &nrElem, i, prag);
    }

    // elimin maximele din vector
    maxime(D, &nrElem);

    // salvez sub forma liniarizata imaginea pe care doresc sa trasez conturul
    Imag = (pixel*)malloc(inaltime*latime*sizeof(pixel));
    if(Imag == NULL) printf("Nu am putut aloca memmorie pentru imaginea data. \n");
    liniarizare(&Imag, imag_init);

    // trasez conturul pentru fiecare cifra in parte
    for(i=0; i<nrElem; i++)
    {
        templat a = D[i];
        contur(Imag,latime,a);
    }

    afisare(Imag,inaltime, latime, imag_color, imag_init);

    free(Imag);
    free(ImagG);
    free(D);
}


int main() {

    int t;
    printf("Meniu functii:\n 1. Criptare\n 2.Testul chi\n 3.Decriptare\n 4.Template matching\n");
    printf("Alegeti un numar: "); scanf("%d", &t);
    while(t>=1 && t<=4)
    {
        switch(t)
        {
            case 1:
            {
                char numeimagSursa[101], numeimagCript[101], fisiertext[101];
                printf("Numele imaginii pe care doriti sa o criptati si numele acesteia dupa criptare: ");
                scanf("%s %s", numeimagSursa, numeimagCript);
                printf("Numele fisierului text ce contine cheia secreta: ");
                scanf("%s", fisiertext);

                criptareImag(numeimagSursa, numeimagCript, fisiertext);
            }break;
            case 2:
            {
                char numeimagSursa[101], numeimagCript[101];
                printf("Numele imaginii initiale si numele acesteia dupa criptare: ");
                scanf("%s %s", numeimagSursa, numeimagCript);

                chi(numeimagSursa);
                chi(numeimagCript);
            }break;
            case 3:
            {
                char numeimagCript[101], numeimagDecript[101], fisiertext[101];
                printf("Numele imaginii pe care doriti sa o decriptati si numele acesteia dupa decriptare: ");
                scanf("%s %s", numeimagCript, numeimagDecript);
                printf("Numele fisierului text ce contine cheia secreta: ");
                scanf("%s", fisiertext);
                decriptareImag(numeimagCript, numeimagDecript, fisiertext);

            }break;
            case 4:
            {
                float prag = 0.7;
                char numeimagSursa[101], numeimagTemp[101], fisier_text[101];

                printf("Numele imaginii sursa este: ");
                scanf("%s", numeimagSursa);
                printf("Numele imaginii dupa template este: ");
                scanf("%s", numeimagTemp);
                printf("Numele fisierului text din care citim numele sabloanelor este: ");
                scanf("%s", fisier_text);

                templateMatching(numeimagSursa, numeimagTemp,fisier_text, prag);
            }break;
        }
        printf("Alegeti un numar: "); scanf("%d", &t);
    }
    return 0;
}

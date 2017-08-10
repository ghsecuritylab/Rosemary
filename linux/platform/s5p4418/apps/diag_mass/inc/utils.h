#ifndef __UTILS_H__
#define	__UTILS_H__

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <nx_diag_type.h>
#include <pthread.h>

class CNX_AutoLock{
public:
    CNX_AutoLock( pthread_mutex_t *pLock ) :m_pLock(pLock)
	{
        pthread_mutex_lock( m_pLock );
    }
    ~CNX_AutoLock()
	{
        pthread_mutex_unlock(m_pLock);
    }
protected:
	pthread_mutex_t	*m_pLock;
private:
    CNX_AutoLock (const CNX_AutoLock &Ref);
    CNX_AutoLock &operator=(CNX_AutoLock &Ref);
};

int KillAppByName( const char *appName );

bool ReadSysFileInfo( const char *path, char *buf, int bufSize );

void DrawButton( SDL_Surface *surface, SDL_Rect *rect, Uint32 color );
void DrawBox( SDL_Surface *surface, SDL_Rect *rect, Uint32 color, Uint32 lineColor );
void DrawLine( SDL_Surface *surface, int x, int y, int w, int h, Uint32 color );
void SetLine( SDL_Surface *surface, int x, int y, int w, int h, Uint32 color );
void DrawString( SDL_Surface *surface, TTF_Font *font, const char *string, SDL_Rect rect );
void DrawString2( SDL_Surface *surface, TTF_Font *font, const char *string, SDL_Rect rect, SDL_Color fgColor, int x_center=1, int y_center=1 );
NXDiagButton *CreateButton( SDL_Rect *rect, BUTTON_STATE state, int action, void *owner );
void DestroyButton( NXDiagButton *btn );

#endif //	__UTILS_H__

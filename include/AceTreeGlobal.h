#ifndef ACETREEGLOBAL_H
#define ACETREEGLOBAL_H

#include <QtGlobal>

#ifndef ACETREE_EXPORT
#    ifdef ACETREE_STATIC
#        define ACETREE_EXPORT
#    else
#        ifdef ACETREE_LIBRARY
#            define ACETREE_EXPORT Q_DECL_EXPORT
#        else
#            define ACETREE_EXPORT Q_DECL_IMPORT
#        endif
#    endif
#endif


#endif // ACETREEGLOBAL_H

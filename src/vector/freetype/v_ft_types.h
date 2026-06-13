#ifndef V_FT_TYPES_H
#define V_FT_TYPES_H

#include <cstdint>

/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_Fixed                                                           */
/*                                                                       */
/* <Description>                                                         */
/*    This type is used to store 16.16 fixed-point values, like scaling  */
/*    values or matrix coefficients.                                     */
/*                                                                       */
typedef int32_t  SW_FT_Fixed;


/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_Int                                                             */
/*                                                                       */
/* <Description>                                                         */
/*    A typedef for the int type.                                        */
/*                                                                       */
typedef int32_t  SW_FT_Int;


/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_UInt                                                            */
/*                                                                       */
/* <Description>                                                         */
/*    A typedef for the uint32_t type.                                   */
/*                                                                       */
typedef uint32_t  SW_FT_UInt;


/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_Long                                                            */
/*                                                                       */
/* <Description>                                                         */
/*    A typedef for int32_t.                                             */
/*                                                                       */
typedef int32_t  SW_FT_Long;


/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_ULong                                                           */
/*                                                                       */
/* <Description>                                                         */
/*    A typedef for uint32_t.                                            */
/*                                                                       */
typedef uint32_t SW_FT_ULong;

/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_Short                                                           */
/*                                                                       */
/* <Description>                                                         */
/*    A typedef for signed short.                                        */
/*                                                                       */
typedef int16_t  SW_FT_Short;


/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_Byte                                                            */
/*                                                                       */
/* <Description>                                                         */
/*    A simple typedef for the _unsigned_ char type.                     */
/*                                                                       */
typedef uint8_t  SW_FT_Byte;


/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_Bool                                                            */
/*                                                                       */
/* <Description>                                                         */
/*    A typedef of unsigned char, used for simple booleans.  As usual,   */
/*    values 1 and~0 represent true and false, respectively.             */
/*                                                                       */
typedef uint8_t  SW_FT_Bool;



/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_Error                                                           */
/*                                                                       */
/* <Description>                                                         */
/*    The FreeType error code type.  A value of~0 is always interpreted  */
/*    as a successful operation.                                         */
/*                                                                       */
typedef int  SW_FT_Error;


/*************************************************************************/
/*                                                                       */
/* <Type>                                                                */
/*    SW_FT_Pos                                                             */
/*                                                                       */
/* <Description>                                                         */
/*    The type SW_FT_Pos is used to store vectorial coordinates.  Depending */
/*    on the context, these can represent distances in integer font      */
/*    units, or 16.16, or 26.6 fixed-point pixel coordinates.            */
/*                                                                       */
typedef int32_t  SW_FT_Pos;


/*************************************************************************/
/*                                                                       */
/* <Struct>                                                              */
/*    SW_FT_Vector                                                          */
/*                                                                       */
/* <Description>                                                         */
/*    A simple structure used to store a 2D vector; coordinates are of   */
/*    the SW_FT_Pos type.                                                   */
/*                                                                       */
/* <Fields>                                                              */
/*    x :: The horizontal coordinate.                                    */
/*    y :: The vertical coordinate.                                      */
/*                                                                       */
typedef struct  SW_FT_Vector_
{
  SW_FT_Pos  x;
  SW_FT_Pos  y;

} SW_FT_Vector;


typedef int64_t             SW_FT_Int64;
typedef uint64_t            SW_FT_UInt64;

typedef int32_t             SW_FT_Int32;
typedef uint32_t            SW_FT_UInt32;


#define SW_FT_BOOL( x )  ( (SW_FT_Bool)( x ) )

#define SW_FT_SIZEOF_LONG 4

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE  0
#endif


#endif // V_FT_TYPES_H

#ifndef _AT_ERROR_H
#define _AT_ERROR_H

//-------------------成功--------------------------------------
#define	AT_SUCESS							0				// 成功
#define	AT_UNKNOWN_ERROR					1				// 未知错误

//-------------------参数错误-------------------------
#define	AT_INVALID_PARAMETER				101				// 请求参数无效
#define AT_TOO_MANY_PARAMETER				102				// 请求参数过多
#define AT_SPECIFIED_OBJECT_CANNOT_FOUND	103				// 指定的对象不存在
#define AT_SPECIFIED_OBJECT_ALREADY_EXIST	104				// 指定的对象已存在

//--------------------数据错误-------------------------
#define AT_UNKNOWN_DATA_STORE_ERROR			201				// 未知的存储操作错误
#define AT_DATA_STORE_EXCEEDED				202				// 数据存储空间已超过设定的上限
#define AT_DATABASE_ERROR					203				// 数据库操作出错，请重试

//--------------------命令错误-------------------------
#define AT_INVALID_OPERATION				301				// 无效的操作方法


//--------------------板卡状态---------------------------
#define AT_CARD_NOT_AVAILABLE				401				// 板卡不存在
#define AT_CONNECT_TIME_OUT					402				// 板卡连接超时
#define AT_CARD_BUSY						403				// 板卡忙
#define AT_CARD_UNKNOWN						404				// 未知错误

#define AT_CHANNEL_NOT_AVAILABLE			405				// 通道不存在
#define AT_CHANNEL_BUSY						406				// 通道忙
#define AT_CHANNEL_UNKNOWN					407				// 未知通道错误
#define AT_CHANNEL_LIMIT					408				// 通道上限



//---------------------用户以及安全----------------------------------------------
#define AT_UNAUTHORIZED_CLIENT				501				// 请求来自未经授权的用户
#define AT_NO_PERMISSION_ACCESS_DATA		502				// 无权限访问数据
#define AT_INVALID_API_KEY					503				// api key无效

#define AT_INCORRECT_SIGNATURE				504				// 无效签名
#define AT_UNSUPPORTED_SIGNATURE_METHOD		505				// 未知的签名方法

#define AT_INVALID_USER_INFO				506				// 无效的用户资料
#define AT_ACCESS_TOKEN_INVALID				507				// 无效的access token

#define AT_ACCESS_TOKEN						508				// 账户过期
#define AT_USER_NOT_VISIBLE					509				// 用户不可见

#endif


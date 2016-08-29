#ifndef _AT_ERROR_H
#define _AT_ERROR_H

//-------------------�ɹ�--------------------------------------
#define	AT_SUCESS							0				// �ɹ�
#define	AT_UNKNOWN_ERROR					1				// δ֪����

//-------------------��������-------------------------
#define	AT_INVALID_PARAMETER				101				// ���������Ч
#define AT_TOO_MANY_PARAMETER				102				// �����������
#define AT_SPECIFIED_OBJECT_CANNOT_FOUND	103				// ָ���Ķ��󲻴���
#define AT_SPECIFIED_OBJECT_ALREADY_EXIST	104				// ָ���Ķ����Ѵ���

//--------------------���ݴ���-------------------------
#define AT_UNKNOWN_DATA_STORE_ERROR			201				// δ֪�Ĵ洢��������
#define AT_DATA_STORE_EXCEEDED				202				// ���ݴ洢�ռ��ѳ����趨������
#define AT_DATABASE_ERROR					203				// ���ݿ��������������

//--------------------�������-------------------------
#define AT_INVALID_OPERATION				301				// ��Ч�Ĳ�������


//--------------------�忨״̬---------------------------
#define AT_CARD_NOT_AVAILABLE				401				// �忨������
#define AT_CONNECT_TIME_OUT					402				// �忨���ӳ�ʱ
#define AT_CARD_BUSY						403				// �忨æ
#define AT_CARD_UNKNOWN						404				// δ֪����

#define AT_CHANNEL_NOT_AVAILABLE			405				// ͨ��������
#define AT_CHANNEL_BUSY						406				// ͨ��æ
#define AT_CHANNEL_UNKNOWN					407				// δ֪ͨ������
#define AT_CHANNEL_LIMIT					408				// ͨ������



//---------------------�û��Լ���ȫ----------------------------------------------
#define AT_UNAUTHORIZED_CLIENT				501				// ��������δ����Ȩ���û�
#define AT_NO_PERMISSION_ACCESS_DATA		502				// ��Ȩ�޷�������
#define AT_INVALID_API_KEY					503				// api key��Ч

#define AT_INCORRECT_SIGNATURE				504				// ��Чǩ��
#define AT_UNSUPPORTED_SIGNATURE_METHOD		505				// δ֪��ǩ������

#define AT_INVALID_USER_INFO				506				// ��Ч���û�����
#define AT_ACCESS_TOKEN_INVALID				507				// ��Ч��access token

#define AT_ACCESS_TOKEN						508				// �˻�����
#define AT_USER_NOT_VISIBLE					509				// �û����ɼ�

#endif


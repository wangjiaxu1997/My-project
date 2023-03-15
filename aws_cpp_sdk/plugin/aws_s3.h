/**************************************************************************    
Copyright: 
File Name: aws_s3.h  
Description: Use Amazon Web Service CPP SDK to upload files
Version: V1.0   
Author: Administrator  
Create Time: 2023-02-16
Other:
**************************************************************************/
#ifndef __UPLOAD_FILES_AWS_S3_H_
#define __UPLOAD_FILES_AWS_S3_H_

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpClient.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/utils/stream/SimpleStreamBuf.h>
#include <aws/core/client/AsyncCallerContext.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
//#include <aws/core/utils/threading/Executor.h>
#include <condition_variable>
#include <mutex>

using namespace Aws::S3;
using namespace Aws::Http;
using namespace Aws::Client;
using namespace Aws::S3::Model;
//using namespace Aws::Utils::Threading;

class CAWSS3Tools {
public:
	CAWSS3Tools();
	~CAWSS3Tools();
	
public:

	int init(const Aws::String  endPoint, const Aws::String  region, const Aws::String  accessKey, const Aws::String  secretKey, int useVirtualAddressing);

	bool PutObject(const Aws::String  bucketName, const Aws::String  objectKey, const Aws::String uuid, const char* fileContents, int contentLen);

	bool PutObjectAsync(const Aws::String  bucketName, const Aws::String  objectKey, const Aws::String uuid, const char* fileContents, int contentLen);

	bool RandomHttpPut(const Aws::String bucketName, const Aws::String objectKey, const Aws::String uuid, const char* fileContents, int contentLen);

private:
	static void PutObjectAsyncFinished(const Aws::S3::S3Client *s3Client,
		const Aws::S3::Model::PutObjectRequest &request,
		const Aws::S3::Model::PutObjectOutcome &outcome,
		const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context);

	bool MakeRandomHttpRequest(std::shared_ptr<Aws::IOStream> sbody, int len, const Aws::String httpUrl);


private:
	S3Client * m_client = { NULL };
	Aws::SDKOptions m_options;
	std::shared_ptr<HttpClient>m_httpClient;
};
#endif // __UPLOAD_FILES_AWS_S3_H_
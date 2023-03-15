/**************************************************************************    
Copyright: 
File Name: aws_s3.cpp  
Description: Use Amazon Web Service CPP SDK to upload files
Version: V1.0   
Author: Administrator  
Create Time: 2023-02-16
Other:
**************************************************************************/

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "aws_s3.h"
#include "Common.h"
#include "Log.h"

std::mutex upload_mutex;
std::condition_variable upload_variable;
#define gettid() syscall(SYS_gettid)

CAWSS3Tools::CAWSS3Tools()
{
}
CAWSS3Tools::~CAWSS3Tools()
{
	if (m_client != NULL)
	{
		delete m_client;
		m_client = NULL;
	}
	Aws::ShutdownAPI(m_options);
}
	
int CAWSS3Tools::init(const Aws::String endPoint, const Aws::String region, const Aws::String accessKey, const Aws::String secretKey, int useVirtualAddressing)
{
	Aws::InitAPI(m_options);

	Aws::Client::ClientConfiguration config;
	config.endpointOverride = endPoint;  // S3服务器地址和端口
	config.scheme = Aws::Http::Scheme::HTTP;
	config.region = region;
	config.verifySSL = false;
	config.connectTimeoutMs = 30000; // 与服务端连接超时时间
	config.requestTimeoutMs = 30000;  // 上传下载请求超时时间
	config.maxConnections = 25;  // 单个http客户端可使用的最大并发tcp连接数 默认值25。
	//config.executor = Aws::MakeShared<Aws::Utils::Threading::DefaultExecutor>("DefaultExecutor");  //默认执行器， 不限制线程数量，来一个请求就建立一个线程处理，不排队
	//config.executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>("PooledThreadExecutor", 10);  //线程池类执行器，第二个参数表示异步处理线程的数量，排队处理

	m_httpClient = Aws::Http::CreateHttpClient(config);
	if (m_httpClient == NULL){
		LOG(LOG_ERROR, "%s HttpClient init failed: endPoint:%s region:%s accessKey%s secretKey:%s", __func__,
			endPoint.c_str(), region.c_str(), accessKey.c_str(), secretKey.c_str());
		return -1;
	}

	m_client = new S3Client(Aws::Auth::AWSCredentials(accessKey, secretKey), config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, (bool)useVirtualAddressing);
	if (m_client == NULL){
		LOG(LOG_ERROR, "%s S3Client failed: endPoint:%s region:%s accessKey%s secretKey:%s", __func__,
			endPoint.c_str(), region.c_str(), accessKey.c_str(), secretKey.c_str());
		return -1;
	}
	return 0;
}

bool CAWSS3Tools::PutObject(const Aws::String bucketName, const Aws::String objectKey, const Aws::String uuid, const char* fileContents, int contentLen)
{
	PutObjectRequest request;
	request.WithKey(objectKey).WithBucket(bucketName);

	auto sbuff = Aws::New<Aws::Utils::Stream::PreallocatedStreamBuf>("CAWSS3Tools", (unsigned char*)fileContents, contentLen);
	auto sbody = Aws::MakeShared<Aws::IOStream>("whatever string", sbuff);
	request.SetBody(sbody);

	auto outcome = m_client->PutObject(request);
	if (outcome.IsSuccess()) {
		return true;
	}
	else {
		LOG(LOG_ERROR, "%s PutObject error: %s %s bucketName:%s objectKey:%s", __func__,
			outcome.GetError().GetExceptionName().c_str(), outcome.GetError().GetMessage().c_str(), bucketName.c_str(), objectKey.c_str());
		return false;
	}
}

bool CAWSS3Tools::PutObjectAsync(const Aws::String bucketName, const Aws::String objectKey, const Aws::String uuid, const char* fileContents, int contentLen)
{
	PutObjectRequest request;
	request.WithKey(objectKey).WithBucket(bucketName);

	//auto sbuff = Aws::New<Aws::Utils::Stream::PreallocatedStreamBuf>("PutObjectAsync", (unsigned char*)fileContents, contentLen);
	auto sbuff = Aws::New<Aws::Utils::Stream::SimpleStreamBuf >("PutObjectAsync");
	sbuff->sputn(fileContents, contentLen);
	auto sbody = Aws::MakeShared<Aws::IOStream>("whatever string", sbuff);
	request.SetBody(sbody);

	std::shared_ptr<Aws::Client::AsyncCallerContext> context = Aws::MakeShared<Aws::Client::AsyncCallerContext>("PutObjectAsync");
	context->SetUUID(uuid);

	std::time_t t = std::time(NULL) + 1200; //设置签名URL有效时长。
	m_client->GeneratePresignedUrl(bucketName, objectKey, Aws::Http::HttpMethod::HTTP_PUT, t); /* 生成签名URL。*/

	//std::unique_lock<std::mutex> lock(upload_mutex);
	m_client->PutObjectAsync(request, PutObjectAsyncFinished, context);
	//upload_variable.wait(lock); //这里等待回调后再往下走 
	return true;
}

void CAWSS3Tools::PutObjectAsyncFinished(const Aws::S3::S3Client *s3Client,
	const Aws::S3::Model::PutObjectRequest &request,
	const Aws::S3::Model::PutObjectOutcome &outcome,
	const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context)
{
	if (outcome.IsSuccess()) {
		aws_upload_callback(context->GetUUID().c_str());
	}
	else {
		LOG(LOG_ERROR, "%s Error:%s. uuid:%s", __func__, outcome.GetError().GetMessage().c_str(), context->GetUUID().c_str());
	}
	//upload_variable.notify_one();
}

bool CAWSS3Tools::RandomHttpPut(const Aws::String bucketName, const Aws::String objectKey, const Aws::String uuid, const char* fileContents, int contentLen)
{
	std::time_t t = std::time(NULL)+1200; //设置签名URL有效时长。
	Aws::String httpUrl = m_client->GeneratePresignedUrl(bucketName, objectKey, Aws::Http::HttpMethod::HTTP_PUT, t); /* 生成签名URL。*/

	auto sbuff = Aws::New<Aws::Utils::Stream::PreallocatedStreamBuf>("CAWSS3Tools", (unsigned char*)fileContents, contentLen);
	auto sbody = Aws::MakeShared<Aws::IOStream>("whatever string", sbuff);

	return MakeRandomHttpRequest(sbody, contentLen, httpUrl);
}

bool CAWSS3Tools::MakeRandomHttpRequest(std::shared_ptr<Aws::IOStream> sbody, int len, const Aws::String httpUrl)
{
	auto request = Aws::Http::CreateHttpRequest(httpUrl, HttpMethod::HTTP_PUT,
		Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);

	Aws::StringStream intConverter;
	intConverter << len;
	request->SetContentLength(intConverter.str());
	request->AddContentBody(sbody);

	auto response = m_httpClient->MakeRequest(request);
	if (response->HasClientError()) {
		LOG(LOG_ERROR, "%s MakeRequest fail errorcode: %d httpUrl:%s", __func__, (int)response->GetResponseCode(), httpUrl.c_str());
		return false;
	}
	else {
		return true;
	}
}
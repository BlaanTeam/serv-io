#include "response_sender.hpp"

#include "response.hpp"

// Thin adapters around the existing Response::setupX / Response::sendX
// methods. Keeping the implementations on Response itself avoids moving
// large chunks of state into the senders during the Strategy extraction;
// the senders provide the protocol-level polymorphism the call site needs.

ResponseSender::~ResponseSender() {}

// -------------------------------------------------------- LengthedSender ---

bool LengthedSender::prepareHeaders(Response &r) {
	r.setupLengthedBody();
	return true;
}

void LengthedSender::sendBody(Response &r, sockfd fd) {
	r.sendLengthedBody(fd);
}

// --------------------------------------------------------- ChunkedSender ---

bool ChunkedSender::prepareHeaders(Response &r) {
	r.setupChunkedBody();
	return true;
}

void ChunkedSender::sendBody(Response &r, sockfd fd) {
	r.sendChunkedBody(fd);
}

// ---------------------------------------------------------- RangedSender ---

bool RangedSender::prepareHeaders(Response &r) {
	r.setupRangedBody();
	return true;
}

void RangedSender::sendBody(Response &r, sockfd fd) {
	r.sendRangedBody(fd);
}

// ------------------------------------------------------------- CGISender ---

bool CGISender::prepareHeaders(Response &r) {
	// CGI cannot finalize headers until the request body has been forwarded
	// to the child (so the child has had a chance to emit its own headers).
	if (!r._req->match(REQ_DONE))
		return false;
	return r.setupCGIBody();
}

void CGISender::sendBody(Response &r, sockfd fd) {
	r.sendCGIBody(fd);
}

// ---------------------------------------------------------- UploadSender ---

bool UploadSender::prepareHeaders(Response &r) {
	return r.setupUploadBody();
}

void UploadSender::sendBody(Response &r, sockfd fd) {
	r.sendUploadBody(fd);
}

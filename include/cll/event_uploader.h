#pragma once

#include <memory>
#include <vector>
#include "event.h"
#include "event_upload_step.h"
#include "event_batch.h"

namespace cll {

struct EventUploadStatus {

    enum class State {
        Success,
        ErrorGeneric, ErrorRateLimit
    };

    State state;
    std::chrono::seconds retryAfter;

    static EventUploadStatus success() {
        return {State::Success};
    }
    static EventUploadStatus error() {
        return {State::ErrorGeneric};
    }
    static EventUploadStatus rateLimit(std::chrono::seconds retryAfter) {
        return {State::ErrorRateLimit, retryAfter};
    }


    operator bool() const {
        return state == State::Success;
    }

};

class EventUploader {

private:
    std::string url;
    std::vector<std::unique_ptr<EventUploadStep>> steps;

public:
    EventUploader(std::string url = "https://vortex.data.microsoft.com/collect/v1") : url(std::move(url)) {}

    EventUploadStatus sendEvents(BatchedEventList& batch, bool canRetry = true);

    EventUploadStatus sendEvents(EventBatch& batch, size_t maxCount, size_t maxSize);

    void addStep(std::unique_ptr<EventUploadStep> step) {
        steps.push_back(std::move(step));
    }

};

}
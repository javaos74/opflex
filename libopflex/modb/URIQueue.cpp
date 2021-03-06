/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for URIQueue class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <boost/foreach.hpp>
#include "opflex/modb/internal/URIQueue.h"

#include "LockGuard.h"
#include "opflex/logging/internal/logging.hpp"

namespace opflex {
namespace modb {

URIQueue::URIQueue(QProcessor* processor_) 
    : processor(processor_), proc_shouldRun(false) {
    uv_mutex_init(&item_mutex);
    uv_cond_init(&item_cond);
}

URIQueue::~URIQueue() {
    stop();
    uv_cond_destroy(&item_cond);
    uv_mutex_destroy(&item_mutex);
}

// listen on the item queue and dispatch events where required
void URIQueue::proc_thread_func(void* queue_) {
    URIQueue* queue = (URIQueue*)queue_;

    while (queue->proc_shouldRun) {
        item_queue_t toProcess;
        {
            util::LockGuard guard(&queue->item_mutex);
            while (queue->item_queue.size() == 0 && queue->proc_shouldRun)
                uv_cond_wait(&queue->item_cond, &queue->item_mutex);
            if (!queue->proc_shouldRun) return;

            toProcess.swap(queue->item_queue);
        }

        BOOST_FOREACH (const URIQueue::item& d, toProcess) {
            if (!queue->proc_shouldRun) return;
            try {
                queue->processor->processItem(d.uri, d.data);
            } catch (const std::exception& ex) {
                LOG(ERROR) << "Exception while processing notification queue: " <<
                    ex.what();
            } catch (...) {
                LOG(ERROR) << "Unknown error while processing notification queue";
            }
        }
    }
}

void URIQueue::start() {
    proc_shouldRun = true;
    uv_thread_create(&proc_thread, proc_thread_func, this);
}

void URIQueue::stop() {
    if (proc_shouldRun) {
        proc_shouldRun = false;
        {
            util::LockGuard guard(&item_mutex);
            uv_cond_signal(&item_cond);
        }
        uv_thread_join(&proc_thread);

        {
            util::LockGuard guard(&item_mutex);
            item_queue.clear();
        }
    }
}

void URIQueue::queueItem(const URI& uri, const boost::any& data) {
    {
        util::LockGuard guard(&item_mutex);
        item_queue.push_back(item(uri, data));
        uv_cond_signal(&item_cond);
    }
}

} /* namespace modb */
} /* namespace opflex */

#include <Windows.h>
#include "activity_clock_push.h"
#include "activity_notification_frame.h"
#include "../../../../runtime/activity/native_activity_runtime.h"
#include "../../../../../middleware/bap/activity_message/native/activity_clock_sync.h"
#include "../../../../../middleware/encoding/bit_writer.h"
#include "../../../../../middleware/secure_channel/runtime.h"

namespace sunrise::server::bap::encrypted::push::activity {
namespace clock=runtime::activity::activity_clock;
namespace wire=middleware::bap::activity_message::native::activity_clock;

void release_borrowed_clock_publication(BorrowedClockPublication& publication) noexcept {
    gameplay::group::release_host_activity_lineage(publication.lease);
    publication={};
}
bool borrowed_clock_publication_is_current(const Session& session,
    const BorrowedClockPublication& publication) noexcept {
    if(!publication.present)return true;
    clock::Publication current{};
    return session.activity.joinedForeignSession
        && session.activity.lineage==publication.edge
        && publication.edge.kind==RegionLineageKind::groupDerivedBorrow
        && publication.edge.bound!=publication.edge.source
        && publication.edge.bound==session.activity.instance
        && publication.domain.owner==publication.edge.source
        && publication.lease.pinned && publication.lease.host==publication.edge.bound
        && publication.lease.source==publication.edge.source
        && retained_region_lineage_is_current_locked(session,publication.edge,publication.lease)
        && runtime::activity::native_activity::snapshot_clock(publication.edge.source,current)
        && current.domain==publication.domain;
}
bool append_borrowed_clock_notifications(const Session& session,Scratch& scratch,
    std::span<const std::byte,state::kAesKeySize> key,
    std::array<std::byte,state::kBapNonceSize>& nonce,std::span<std::byte> response,
    std::size_t& written,BorrowedClockPublication& publication) noexcept {
    if(publication.present || publication.lease.pinned || written>response.size())return false;
    // Preserve the previous keepalive bytes until a real type52 epoch and an
    // explicit borrowed creator relationship exist. No guessed public SOID.
    if(!session.activity.joinedForeignSession || !session.activityPatchEpochSeen
        || session.activity.lineage.kind!=RegionLineageKind::groupDerivedBorrow)return true;
    const auto edge=session.activity.lineage;
    if(!edge || edge.bound!=session.activity.instance || edge.bound==edge.source)return false;
    if(!acquire_region_lineage_locked(session,edge,publication.lease)) {
        release_borrowed_clock_publication(publication);return false;
    }
    clock::Publication source{};
    if(!runtime::activity::native_activity::snapshot_clock(edge.source,source)) {
        release_borrowed_clock_publication(publication);return true;
    }
    publication.edge=edge;publication.domain=source.domain;publication.present=true;
    if(!borrowed_clock_publication_is_current(session,publication)) {
        release_borrowed_clock_publication(publication);return false;
    }
    const auto initialWritten=written;
    auto initialNonce=nonce;
    std::array<std::byte,5> configuration{};
    std::array<std::byte,wire::kSynchronizationBytes> synchronization{};
    middleware::encoding::bits::Writer configWriter(configuration),syncWriter(synchronization);
    std::size_t configBytes{},syncBytes{};
    bool encoded=wire::write(configWriter,source.configuration)
        && configWriter.finish(configBytes) && configBytes==configuration.size()
        && wire::write_synchronization(syncWriter,session.activityPatchEpoch,source.elapsedTicks)
        && syncWriter.bit_count()==wire::kSynchronizationBits
        && syncWriter.finish(syncBytes) && syncBytes==synchronization.size()
        && append_notification_frame(scratch,edge.bound.sessionId,wire::kMessageType,
            configuration,key,nonce,response,written);
    if(encoded) {
        middleware::secure_channel::advance_nonce(nonce);
        encoded=append_notification_frame(scratch,edge.bound.sessionId,wire::kSynchronizationMessageType,
            synchronization,key,nonce,response,written);
        if(encoded)middleware::secure_channel::advance_nonce(nonce);
    }
    encoded=encoded && borrowed_clock_publication_is_current(session,publication);
    if(!encoded) {
        if(written>initialWritten)SecureZeroMemory(response.data()+initialWritten,written-initialWritten);
        written=initialWritten;nonce=initialNonce;
        release_borrowed_clock_publication(publication);
    }
    SecureZeroMemory(&initialNonce,sizeof initialNonce);
    return encoded;
}
}

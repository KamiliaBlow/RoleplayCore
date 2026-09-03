/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this
 * program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WORLDSERVER_RESOURCES_SERVICE_H
#define WORLDSERVER_RESOURCES_SERVICE_H

#include "WorldserverService.h"
#include "Client/resource_service.pb.h"

namespace Battlenet::Services
{
    // The client resolves shared data blobs through the resources service: it asks for a
    // content handle by (program, stream) and then downloads the blob from the depot URL
    // carried in the handle. One of those blobs feeds the profanity filter dictionary:
    // while the dictionary is empty, the recruitment-description sanitizer inside
    // C_ClubFinder.PostClub substitutes the description with an empty string, so guilds
    // can never submit recruitment text. Serving the handles observed on retail lets the
    // client fetch the blob and populate the dictionary on its own.
    class ResourcesService : public WorldserverService<resources::v1::ResourcesService>
    {
        typedef WorldserverService<resources::v1::ResourcesService> BaseService;

    public:
        ResourcesService(WorldSession* session);

        uint32 HandleGetContentHandle(resources::v1::ContentHandleRequest const* request, bgs::protocol::ContentHandle* response,
            std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& continuation) override;
    };
}

#endif // WORLDSERVER_RESOURCES_SERVICE_H

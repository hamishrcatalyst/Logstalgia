/*
    Copyright (C) 2008 Andrew Caudwell (acaudwell@gmail.com)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "requestball.h"
#include "settings.h"
#include "textarea.h"
#include "logentry.h"

RequestBall::RequestBall(LogEntry* le, const vec3& colour, const vec2& pos, const vec2& dest)
    : le(le), pos(pos), dest(dest), colour(colour) {

    dir = glm::normalize(dest - pos);

    int bytes = le->response_size;
    size = log((float)bytes) + 1.0f;
    if(size<5.0f) size = 5.0f;

    has_bounced = false;
    no_bounce   = !le->successful;

    distance_travelled = 0.0f;
    total_distance = 0.0;

    points.push_back(pos);
    addPoint(dest);

    float halfsize = size * 0.5f;
    offset = vec2(halfsize, halfsize);
}

RequestBall::~RequestBall() {
    delete le;
}

void RequestBall::addPoint(const vec2& p) {
    float line_length = glm::length(points.back() - p);
    total_distance += line_length;
    points.push_back(p);
    line_lengths.push_back(line_length);
}

void RequestBall::changeDestX(float dest_x) {
    if(has_bounced) return;

    if(dest_x <= pos.x) {
        bounce();
        return;
    }

    float t = (dir.y / dir.x);

    vec2 start = points[0];

    float a = t * (dest_x - start.x);
    float y = start.y + a;

    dest = vec2(dest_x, y);

    total_distance = 0.0f;
    line_lengths.clear();

    points.clear();
    points.push_back(start);
    addPoint(dest);
}

void RequestBall::project() {
    distance_travelled = 0.0f;
    total_distance     = 0.0f;

    vec2 target = dest;

    if(!no_bounce) {
        dir.x  = -dir.x;
        target.x = 0;
    } else {
        target.x = display.width;
    }

    points.clear();
    line_lengths.clear();

    points.push_back(pos);

    // tan = o / a
    // o = tan * a
    // a = o / tan

    float t = (dir.y / dir.x);

    float a = (target.x - pos.x);
    float y = pos.y + t * a;

    if(y < offset.y || y > display.height-offset.y) {

        // bounced off the top/bottom of screen

        float intersect_y = y <= offset.y ? offset.y : display.height-offset.y;

        float o = (intersect_y - pos.y);
        float x = pos.x + (o / t);

        vec2 intersect = vec2(x, intersect_y);

        addPoint(intersect);

        // continue from bounce to destination

        vec2 bounce_dir = dir;

        bounce_dir.y = -bounce_dir.y;

        t = (bounce_dir.y / bounce_dir.x);

        a = t * (target.x - intersect.x);
        y = intersect.y + a;

        intersect = vec2(target.x, y);

        addPoint(intersect);
    } else {
        vec2 intersect = vec2(target.x, y);
        addPoint(intersect);
    }
}

bool RequestBall::isFinished() const {
    return has_bounced && distance_travelled >= total_distance;
}

void RequestBall::bounce() {
    if(has_bounced) return;

    project();

    has_bounced=true;
}

float RequestBall::arrivalTime() {
    return (total_distance-distance_travelled) / (settings.pitch_speed * (float) display.width);
}

float RequestBall::getProgress() const {
    return (distance_travelled/total_distance);
}

const vec2& RequestBall::getFinishPos() const {
    return points.back();
}

bool RequestBall::hasBounced() const {
    return has_bounced;
}

const vec3& RequestBall::getColour() const {
    return colour;
}

LogEntry* RequestBall::getLogEntry() const {
    return le;
}

void RequestBall::formatRequestDetail(LogEntry* le, std::vector<std::string>& content) {

    std::vector<std::string> fields = settings.display_fields;
    if(fields.empty()) fields = LogEntry::getDefaultFields();

    size_t longest_title = 0;
    for(const std::string& field : fields) {
        longest_title = std::max( longest_title, le->getFieldTitle(field).size() );
    }

    for(const std::string& field : fields) {

        std::string title = le->getFieldTitle(field);

        int title_padding_length = longest_title - title.size();

        if(title_padding_length > 0) {
            title += std::string().append(title_padding_length, ' ');
        }

        std::string value;
        le->getValue(field, value);

        std::string overflow_padding(title.size() + 1, ' ');

        // TextArea::setText throws away characters after the first 100.
        unsigned int max_value_length = 100 - overflow_padding.size();

        if(!value.empty()) {
            content.push_back(title + std::string(" ") + value.substr(0, max_value_length));

            for(unsigned int i = max_value_length; i < value.length(); i += max_value_length) {
                content.push_back(overflow_padding + value.substr(i, max_value_length));
            }
        }
    }
}

bool RequestBall::mouseOver(TextArea& textarea, vec2& mouse) {

    //within 3 pixels
    vec2 from_mouse = pos - mouse;

    if( glm::dot(from_mouse, from_mouse) < 36.0f) {

        std::vector<std::string> content;
        formatRequestDetail(le, content);

        textarea.setText(content);
        textarea.setPos(mouse);
        textarea.setColour(colour);
        return true;
    }

    return false;
}

void RequestBall::animate(float dt) {
    distance_travelled += dt * settings.pitch_speed * (float) display.width;

    if(distance_travelled >= total_distance) {

        if(!has_bounced) {
            bounce();
        }
        return;
    }

    //number of lines
    int nolines = points.size()-1;

    int pointno = 0;
    float len=0;

    while(pointno < nolines && len+line_lengths[pointno] < distance_travelled) {
        len += line_lengths[pointno];
        pointno++;
    }

    if(pointno>=nolines) {

        if(!has_bounced) {
            bounce();
            return;
        }

        return;
    }

    vec2 from = points[pointno];
    vec2 to   = points[pointno+1];

    float linepos = (distance_travelled - len)/line_lengths[pointno];

    this->pos = from + ((to-from)*linepos);
}

int RequestBall::logic(float dt) {
    float old_x = pos.x;

    animate(dt);

    //returns 1 if just became visible (for score incrementing)
    return (old_x<0.0f && pos.x>=0.0f);
}

void RequestBall::drawGlow() const {
    if(!has_bounced) return;

    float prog = getProgress();

    float glow_radius = size * size * settings.glow_multiplier;

    float alpha = std::min(1.0f, 1.0f-(prog/settings.glow_duration)) * settings.glow_intensity;

    if(alpha <=0.001f) return;
    
    vec3 glow_col = colour * alpha;

    glColor4f(glow_col.x, glow_col.y, glow_col.z, 1.0f);

    glPushMatrix();
        glTranslatef(pos.x, pos.y, 0.0f);

        glBegin(GL_QUADS);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(glow_radius,glow_radius);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(glow_radius,-glow_radius);
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(-glow_radius,-glow_radius);
            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(-glow_radius,glow_radius);
        glEnd();
    glPopMatrix();
}

void RequestBall::draw() const {

    if(!settings.no_bounce || !has_bounced || no_bounce) {

        vec2 offsetpos = pos - offset;

        glColor4f(colour.x, colour.y, colour.z, 1.0f);

        glPushMatrix();
            glTranslatef(offsetpos.x, offsetpos.y, 0.0f);

            glBegin(GL_QUADS);
                glTexCoord2f(0.0f,0.0f);
                glVertex2f(0.0f, 0.0f);

                glTexCoord2f(1.0f,0.0f);
                glVertex2f(size, 0.0f);

                glTexCoord2f(1.0f,1.0f);
                glVertex2f(size, size);

                glTexCoord2f(0.0f,1.0f);
                glVertex2f(0.0f, size);
            glEnd();
        glPopMatrix();
    }
}

void RequestBall::drawResponseCode(FXFont* font) const {
    float prog = getProgress();

    float alpha = 1.0f - std::min(1.0f, prog * 2.0f);

    if(alpha<=0.001f) return;
    
    float drift = prog * 100.0f;

    vec2 msgpos = (dir * drift) + vec2(dest.x-45.0f, dest.y);
    
    font->setColour(vec4(le->response_colour.x, le->response_colour.y, le->response_colour.z, alpha));
    font->draw(msgpos.x, msgpos.y, le->response_code);
}

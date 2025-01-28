from fastapi import FastAPI, Response, status, HTTPException
from fastapi.params import Body
from pydantic import BaseModel
from typing import Optional
from random import randrange

app = FastAPI()

class Post(BaseModel):
    title: str
    content: str
    published: bool = False
    workload: dict = None
    rating: Optional[int] = None

my_posts = [{"title": "Efficient", 
            "content": "humans and birds", 
            'id': 1
            }, 
            {"title": "assholes", 
            "content": "all known as arseholes", 
            'id': 2
            }, 
            {"title": "Psy", 
            "content": "Columbia", 
            'id': 3
            }
            ]

@app.get("/")
async def main():
    return "Fuck Off. I don't want any sort of intrusion in my life by anyone."

@app.post("/posts", status_code=status.HTTP_201_CREATED)
async def create_posts(post: Post):
    post_dict = post.model_dump()
    post_dict['id'] = randrange(0, 1000000)
    my_posts.append(post_dict)
    print(my_posts)
    return {"data": post_dict}

@app.get("/posts/{id}")
async def get_one_post(id: int):
    if id > 3:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, 
                            detail="Fuck you shithead!!")
    return f'id {id} accessed.'


def find_post_index(id):
    for i, p in enumerate(my_posts):
        if id == p['id']:
            return i
    return None

@app.delete('/posts/{id}', status_code=status.HTTP_204_NO_CONTENT)
async def delete_post(id: int):
    index = find_post_index(id)
    if index is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, 
            detail="The id you're accessing is not in my posts"
        )
    my_posts.pop(index)
    # return my_posts
    return {"message": f"Post with id {id} was successfully deleted."}

@app.put(path="/posts/{id}")
async def update_post(id: int, post: Post):
    if id not in [post['id'] for post in my_posts]:
        print([post['id'] for post in my_posts])
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="The post you want to update is not in my posts")
    index = find_post_index(id)
    post_dict = post.model_dump()
    post_dict['id'] = id
    my_posts[index] = post_dict
    return my_posts
